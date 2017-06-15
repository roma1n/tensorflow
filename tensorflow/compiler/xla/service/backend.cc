/* Copyright 2017 The TensorFlow Authors. All Rights Reserved.

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================*/

#include "tensorflow/compiler/xla/service/backend.h"

#include <algorithm>
#include <string>
#include <utility>

#define EIGEN_USE_THREADS

#include "third_party/eigen3/unsupported/Eigen/CXX11/Tensor"
#include "tensorflow/compiler/xla/legacy_flags/backend_flags.h"
#include "tensorflow/compiler/xla/service/compiler.h"
#include "tensorflow/compiler/xla/service/platform_util.h"
#include "tensorflow/compiler/xla/status_macros.h"
#include "tensorflow/compiler/xla/statusor.h"
#include "tensorflow/compiler/xla/types.h"
#include "tensorflow/compiler/xla/util.h"
#include "tensorflow/core/common_runtime/eigen_thread_pool.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/lib/core/threadpool.h"
#include "tensorflow/core/platform/cpu_info.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/stream_executor_no_cuda.h"

namespace se = ::perftools::gputools;

namespace xla {

BackendOptions& BackendOptions::set_platform(
    perftools::gputools::Platform* platform) {
  platform_ = platform;
  return *this;
}

perftools::gputools::Platform* BackendOptions::platform() const {
  return platform_;
}

BackendOptions& BackendOptions::set_number_of_replicas(int number_of_replicas) {
  number_of_replicas_ = number_of_replicas;
  return *this;
}

int BackendOptions::number_of_replicas() const { return number_of_replicas_; }

BackendOptions& BackendOptions::set_intra_op_parallelism_threads(
    int num_threads) {
  intra_op_parallelism_threads_ = num_threads;
  return *this;
}

int BackendOptions::intra_op_parallelism_threads() const {
  return intra_op_parallelism_threads_;
}

// Define this in .cc file to avoid having to include eigen or forward declare
// these types in the header.
struct Backend::EigenThreadPoolWrapper {
  explicit EigenThreadPoolWrapper(const int num_threads)
      : pool(new tensorflow::thread::ThreadPool(tensorflow::Env::Default(),
                                                "XLAEigen", num_threads)),
        wrapper(new tensorflow::EigenThreadPoolWrapper(pool.get())),
        device(new Eigen::ThreadPoolDevice(wrapper.get(),
                                           wrapper->NumThreads())) {}

  std::unique_ptr<tensorflow::thread::ThreadPool> pool;
  std::unique_ptr<tensorflow::EigenThreadPoolWrapper> wrapper;
  std::unique_ptr<Eigen::ThreadPoolDevice> device;
};

/* static */ StatusOr<std::unique_ptr<Backend>> Backend::CreateBackend(
    const BackendOptions& options) {
  int64 replica_count = options.number_of_replicas();
  if (replica_count == -1) {
    legacy_flags::BackendFlags* flags = legacy_flags::GetBackendFlags();
    replica_count = flags->xla_replicas;
  }
  perftools::gputools::Platform* platform = options.platform();
  TF_ASSIGN_OR_RETURN(auto compiler, Compiler::GetForPlatform(platform));
  TF_ASSIGN_OR_RETURN(auto stream_executors,
                      PlatformUtil::GetStreamExecutors(platform));
  TF_ASSIGN_OR_RETURN(auto transfer_manager,
                      TransferManager::GetForPlatform(platform));
  TF_ASSIGN_OR_RETURN(auto computation_placer,
                      ComputationPlacer::GetForPlatform(platform));
  std::unique_ptr<Backend> backend(new Backend(
      replica_count, platform, compiler, stream_executors, transfer_manager,
      computation_placer, options.intra_op_parallelism_threads()));
  return std::move(backend);
}

/* static */ StatusOr<std::unique_ptr<Backend>>
Backend::CreateDefaultBackend() {
  TF_ASSIGN_OR_RETURN(se::Platform * platform,
                      PlatformUtil::GetDefaultPlatform());
  BackendOptions backend_options;
  backend_options.set_platform(platform);
  return CreateBackend(backend_options);
}

StatusOr<Backend::StreamPtr> Backend::BorrowStream(int device_ordinal) {
  TF_ASSIGN_OR_RETURN(auto exec, stream_executor(device_ordinal));
  return BorrowStream(exec);
}

StatusOr<Backend::StreamPtr> Backend::BorrowStream(
    se::StreamExecutor* executor) {
  tensorflow::mutex_lock l(mu_);
  if (0 == stream_pools_.count(executor)) {
    stream_pools_.emplace(std::piecewise_construct,
                          std::forward_as_tuple(executor),
                          std::forward_as_tuple([executor]() {
                            auto stream = MakeUnique<se::Stream>(executor);
                            stream->Init();
                            return stream;
                          }));
  }
  return stream_pools_.at(executor).Allocate();
}

Backend::Backend(
    int64 replica_count, perftools::gputools::Platform* platform,
    Compiler* compiler,
    tensorflow::gtl::ArraySlice<se::StreamExecutor*> stream_executors,
    TransferManager* transfer_manager, ComputationPlacer* computation_placer,
    int intra_op_parallelism_threads)
    : platform_(platform),
      compiler_(compiler),
      transfer_manager_(transfer_manager),
      computation_placer_(computation_placer),
      replica_count_(replica_count) {
  // The given set of stream executors set may include invalid executors.
  for (se::StreamExecutor* exec : stream_executors) {
    if (exec != nullptr) {
      stream_executors_.push_back(exec);
    }
  }
  CHECK_GE(replica_count, 1) << "Must request at least 1 replica.";

  // Create a memory allocator for the valid stream executors.
  memory_allocator_ =
      MakeUnique<StreamExecutorMemoryAllocator>(platform, stream_executors);

  // First check that there are some non-null stream executors to avoid issuing
  // an error mentioning replicas in the common case of requesting just 1
  // replica, which means no replication.
  CHECK(!stream_executors_.empty())
      << "Service found no devices for backend " << platform_->Name() << '.';
  CHECK_GE(stream_executors_.size(), replica_count)
      << "Requested more replicas than there are devices for backend "
      << platform_->Name() << '.';

  if (platform->id() == se::host::kHostPlatformId) {
    inter_op_thread_pool_.reset(new tensorflow::thread::ThreadPool(
        tensorflow::Env::Default(), "xla_inter_op",
        tensorflow::port::NumSchedulableCPUs()));
    const int num_threads = intra_op_parallelism_threads > 0
                                ? intra_op_parallelism_threads
                                : tensorflow::port::NumSchedulableCPUs();
    intra_op_thread_pool_wrapper_.reset(
        new EigenThreadPoolWrapper(num_threads));
  }
}

Backend::~Backend() {}

int Backend::default_device_ordinal() const {
  return default_stream_executor()->device_ordinal();
}

tensorflow::thread::ThreadPool* Backend::inter_op_thread_pool() const {
  return inter_op_thread_pool_.get();
}

const Eigen::ThreadPoolDevice* Backend::eigen_intra_op_thread_pool_device()
    const {
  if (intra_op_thread_pool_wrapper_ == nullptr) {
    return nullptr;
  }
  return intra_op_thread_pool_wrapper_->device.get();
}

tensorflow::thread::ThreadPool* Backend::eigen_intra_op_thread_pool() const {
  if (intra_op_thread_pool_wrapper_ == nullptr) {
    return nullptr;
  }
  return intra_op_thread_pool_wrapper_->pool.get();
}

StatusOr<perftools::gputools::StreamExecutor*> Backend::stream_executor(
    int device_ordinal) const {
  if (device_ordinal < 0 ||
      device_ordinal > stream_executors_.back()->device_ordinal()) {
    return InvalidArgument(
        "Invalid device ordinal value (%d). Valid range is [0, %d].",
        device_ordinal, stream_executors_.back()->device_ordinal());
  }
  for (auto* executor : stream_executors_) {
    if (executor->device_ordinal() == device_ordinal) {
      return executor;
    }
  }
  return InvalidArgument("device %s not supported by XLA service",
                         device_name(device_ordinal).c_str());
}

StatusOr<bool> Backend::devices_equivalent(int device_ordinal_a,
                                           int device_ordinal_b) {
  // Use the name from device description to determine equivalence. This is a
  // bit crude but works for GPUs which is the important case where we compile
  // an executable for one GPU and want to know if it will run (well) on
  // another.
  TF_ASSIGN_OR_RETURN(perftools::gputools::StreamExecutor * executor_a,
                      stream_executor(device_ordinal_a));
  TF_ASSIGN_OR_RETURN(perftools::gputools::StreamExecutor * executor_b,
                      stream_executor(device_ordinal_b));
  return (executor_a->GetDeviceDescription().name() ==
          executor_b->GetDeviceDescription().name());
}

Status Backend::ResetDevices() {
  return transfer_manager_->ResetDevices(stream_executors_);
}

}  // namespace xla

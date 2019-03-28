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

#ifndef TENSORFLOW_CORE_UTIL_GPU_LAUNCH_CONFIG_H_
#define TENSORFLOW_CORE_UTIL_GPU_LAUNCH_CONFIG_H_

#if GOOGLE_CUDA || TENSORFLOW_USE_ROCM

#include <algorithm>

#include "absl/base/casts.h"
#include "third_party/eigen3/unsupported/Eigen/CXX11/Tensor"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/stream_executor.h"
#include "tensorflow/core/platform/types.h"

// Usage of GetCudaLaunchConfig, GetCuda2DLaunchConfig, and
// GetCuda3DLaunchConfig:
//
// There are two versions of GetCudaLaunchConfig and GetCuda2DLaunchConfig, one
// version uses heuristics without any knowledge of the device kernel, the other
// version uses cudaOccupancyMaxPotentialBlockSize to determine the theoretical
// launch parameters that maximize occupancy. Currently, only the maximum
// occupancy version of GetCuda3DLaunchConfig is available.
//
// For large number of work elements, the convention is that each kernel would
// iterate through its assigned range. The return value of GetCudaLaunchConfig
// is struct CudaLaunchConfig, which contains all the information needed for the
// kernel launch, including: virtual number of threads, the number of threads
// per block and number of threads per block used inside <<< >>> of a kernel
// launch. GetCuda2DLaunchConfig and GetCuda3DLaunchConfig does the same thing
// as CudaLaunchConfig. The only difference is the dimension. The macros
// CUDA_1D_KERNEL_LOOP and CUDA_AXIS_KERNEL_LOOP might be used to do inner loop.
//
/* Sample code:

__global__ void MyKernel1D(CudaLaunchConfig config, other_args...) {
  CUDA_1D_KERNEL_LOOP(x, config.virtual_thread_count) {
    do_your_job_here;
  }
}

__global__ void MyKernel2D(Cuda2DLaunchConfig config, other_args...) {
  CUDA_AXIS_KERNEL_LOOP(x, config.virtual_thread_count, x) {
    CUDA_AXIS_KERNEL_LOOP(y, config.virtual_thread_count, y) {
      do_your_job_here;
    }
  }
}

__global__ void MyKernel3D(Cuda3DLaunchConfig config, other_args...) {
  CUDA_AXIS_KERNEL_LOOP(x, config.virtual_thread_count, x) {
    CUDA_AXIS_KERNEL_LOOP(y, config.virtual_thread_count, y) {
      CUDA_AXIS_KERNEL_LOOP(z, config.virtual_thread_count, z) {
        do_your_job_here;
      }
    }
  }
}

void MyDriverFunc(const Eigen::GpuDevice &d) {
  // use heuristics
  CudaLaunchConfig cfg1 = GetCudaLaunchConfig(10240, d);
  MyKernel1D <<<config.block_count,
                config.thread_per_block, 0, d.stream()>>> (cfg1, other_args...);
  Cuda2DLaunchConfig cfg2 = GetCuda2DLaunchConfig(10240, 10240, d);
  MyKernel2D <<<config.block_count,
                config.thread_per_block, 0, d.stream()>>> (cfg2, other_args...);
  Cuda3DLaunchConfig cfg3 = GetCuda3DLaunchConfig(4096, 4096, 100, d);
  MyKernel3D <<<config.block_count,
                config.thread_per_block, 0, d.stream()>>> (cfg3, other_args...);

  // maximize occupancy
  CudaLaunchConfig cfg4 = GetCudaLaunchConfig(10240, d, MyKernel1D, 0, 0 );
  MyKernel1D <<<config.block_count,
                config.thread_per_block, 0, d.stream()>>> (cfg4, other_args...);
  Cuda2DLaunchConfig cfg5 = GetCuda2DLaunchConfig(10240, 10240, d,
                                                  MyKernel1D, 0, 0);
  MyKernel2D <<<config.block_count,
                config.thread_per_block, 0, d.stream()>>> (cfg5, other_args...);
  Cuda3DLaunchConfig cfg6 = GetCuda3DLaunchConfig(4096, 4096, 100, d,
                                                  MyKernel1D, 0, 0);
  MyKernel3D <<<config.block_count,
                config.thread_per_block, 0, d.stream()>>> (cfg6, other_args...);
}

// See the test for this for more example:
//
https://github.com/tensorflow/tensorflow/blob/master/tensorflow/core/util/gpu_kernel_helper_test.cu.cc

*/

namespace tensorflow {

inline int DivUp(int a, int b) { return (a + b - 1) / b; }

struct CudaLaunchConfig {
  // Logical number of thread that works on the elements. If each logical
  // thread works on exactly a single element, this is the same as the working
  // element count.
  int virtual_thread_count = -1;
  // Number of threads per block.
  int thread_per_block = -1;
  // Number of blocks for Cuda kernel launch.
  int block_count = -1;
};

// Calculate the Cuda launch config we should use for a kernel launch.
// This is assuming the kernel is quite simple and will largely be
// memory-limited.
// REQUIRES: work_element_count > 0.
inline CudaLaunchConfig GetCudaLaunchConfig(int work_element_count,
                                            const Eigen::GpuDevice& d) {
  CHECK_GT(work_element_count, 0);
  CudaLaunchConfig config;
  const int virtual_thread_count = work_element_count;
  const int physical_thread_count = std::min(
      d.getNumGpuMultiProcessors() * d.maxGpuThreadsPerMultiProcessor(),
      virtual_thread_count);
  const int thread_per_block = std::min(1024, d.maxGpuThreadsPerBlock());
  const int block_count =
      std::min(DivUp(physical_thread_count, thread_per_block),
               d.getNumGpuMultiProcessors());

  config.virtual_thread_count = virtual_thread_count;
  config.thread_per_block = thread_per_block;
  config.block_count = block_count;
  return config;
}

// Calculate the Cuda launch config we should use for a kernel launch. This
// variant takes the resource limits of func into account to maximize occupancy.
// REQUIRES: work_element_count > 0.
template <typename DeviceFunc>
inline CudaLaunchConfig GetCudaLaunchConfig(int work_element_count,
                                            const Eigen::GpuDevice& d,
                                            DeviceFunc func,
                                            size_t dynamic_shared_memory_size,
                                            int block_size_limit) {
  CHECK_GT(work_element_count, 0);
  CudaLaunchConfig config;
  int block_count = 0;
  int thread_per_block = 0;

#if GOOGLE_CUDA
  cudaError_t err = cudaOccupancyMaxPotentialBlockSize(
      &block_count, &thread_per_block, func, dynamic_shared_memory_size,
      block_size_limit);
  CHECK_EQ(err, cudaSuccess);
#elif TENSORFLOW_USE_ROCM
  // ROCM TODO re-enable this after hipOccupancyMaxPotentialBlockSize is
  // implemented
  //hipError_t err = hipOccupancyMaxPotentialBlockSize(
  //    &block_count, &thread_per_block, func, dynamic_shared_memory_size,
  //    block_size_limit);
  //CHECK_EQ(err, hipSuccess);

  // Apply the heuristic in GetGpuLaunchConfig(int, const Eigen::GpuDevice&)
  // that the kernel is quite simple and will largely be memory-limited.
  const int physical_thread_count = std::min(
      d.getNumGpuMultiProcessors() * d.maxGpuThreadsPerMultiProcessor(),
      work_element_count);
  // Assume the kernel be simple enough that it is okay to use 1024 threads
  // per workgroup.
  thread_per_block = std::min(1024, d.maxGpuThreadsPerBlock());
  block_count =
      std::min(DivUp(physical_thread_count, thread_per_block),
               d.getNumGpuMultiProcessors());
#endif

  block_count =
      std::min(block_count, DivUp(work_element_count, thread_per_block));

  config.virtual_thread_count = work_element_count;
  config.thread_per_block = thread_per_block;
  config.block_count = block_count;
  return config;
}

// Calculate the Cuda launch config we should use for a kernel launch. This
// variant takes the resource limits of func into account to maximize occupancy.
// The returned launch config has thread_per_block set to fixed_block_size.
// REQUIRES: work_element_count > 0.
template <typename DeviceFunc>
inline CudaLaunchConfig GetCudaLaunchConfigFixedBlockSize(
    int work_element_count, const Eigen::GpuDevice& d, DeviceFunc func,
    size_t dynamic_shared_memory_size, int fixed_block_size) {
  CHECK_GT(work_element_count, 0);
  CudaLaunchConfig config;
  int block_count = 0;

#if GOOGLE_CUDA
  cudaError_t err = cudaOccupancyMaxActiveBlocksPerMultiprocessor(
      &block_count, func, fixed_block_size, dynamic_shared_memory_size);
  CHECK_EQ(err, cudaSuccess);
  block_count = std::min(block_count * d.getNumGpuMultiProcessors(),
                         DivUp(work_element_count, fixed_block_size));
#elif TENSORFLOW_USE_ROCM
  // ROCM TODO re-enable this after hipOccupancyMaxActiveBlocksPerMultiprocessor is
  // implemented
  //hipError_t err = hipOccupancyMaxActiveBlocksPerMultiprocessor(
  //    &block_count, &thread_per_block, func, dynamic_shared_memory_size,
  //    block_size_limit);
  //CHECK_EQ(err, hipSuccess);

  // Apply the heuristic in GetGpuLaunchConfig(int, const Eigen::GpuDevice&)
  // that the kernel is quite simple and will largely be memory-limited.
  const int physical_thread_count = std::min(
      d.getNumGpuMultiProcessors() * d.maxGpuThreadsPerMultiProcessor(),
      work_element_count);
  // Assume the kernel be simple enough that it is okay to use 1024 threads
  // per workgroup.
  int thread_per_block = std::min(1024, d.maxGpuThreadsPerBlock());
  block_count =
      std::min(DivUp(physical_thread_count, thread_per_block),
               d.getNumGpuMultiProcessors());
#endif

  config.virtual_thread_count = work_element_count;
  config.thread_per_block = fixed_block_size;
  config.block_count = block_count;
  return config;
}

struct Cuda2DLaunchConfig {
  dim3 virtual_thread_count = dim3(0, 0, 0);
  dim3 thread_per_block = dim3(0, 0, 0);
  dim3 block_count = dim3(0, 0, 0);
};

inline Cuda2DLaunchConfig GetCuda2DLaunchConfig(int xdim, int ydim,
                                                const Eigen::GpuDevice& d) {
  Cuda2DLaunchConfig config;

  if (xdim <= 0 || ydim <= 0) {
    return config;
  }

  const int kThreadsPerBlock = 256;
  int block_cols = std::min(xdim, kThreadsPerBlock);
  // ok to round down here and just do more loops in the kernel
  int block_rows = std::max(kThreadsPerBlock / block_cols, 1);

  const int physical_thread_count =
      d.getNumGpuMultiProcessors() * d.maxGpuThreadsPerMultiProcessor();

  const int max_blocks = std::max(physical_thread_count / kThreadsPerBlock, 1);

  config.virtual_thread_count = dim3(xdim, ydim, 1);
  config.thread_per_block = dim3(block_cols, block_rows, 1);

  int grid_x = std::min(DivUp(xdim, block_cols), max_blocks);

  config.block_count = dim3(
      grid_x, std::min(max_blocks / grid_x, std::max(ydim / block_rows, 1)), 1);
  return config;
}

// Calculate the Cuda 2D and 3D launch config we should use for a kernel launch.
// This variant takes the resource limits of func into account to maximize
// occupancy.
using Cuda3DLaunchConfig = Cuda2DLaunchConfig;

template <typename DeviceFunc>
inline Cuda3DLaunchConfig GetCuda3DLaunchConfig(
    int xdim, int ydim, int zdim, const Eigen::GpuDevice& d, DeviceFunc func,
    size_t dynamic_shared_memory_size, int block_size_limit) {
  Cuda3DLaunchConfig config;

  if (xdim <= 0 || ydim <= 0 || zdim <= 0) {
    return config;
  }

  int dev;
#if GOOGLE_CUDA
  cudaGetDevice(&dev);
  cudaDeviceProp deviceProp;
  cudaGetDeviceProperties(&deviceProp, dev);
#elif TENSORFLOW_USE_ROCM
  hipGetDevice(&dev);
  hipDeviceProp_t deviceProp;
  hipGetDeviceProperties(&deviceProp, dev);
#endif
  int xthreadlimit = deviceProp.maxThreadsDim[0];
  int ythreadlimit = deviceProp.maxThreadsDim[1];
  int zthreadlimit = deviceProp.maxThreadsDim[2];
  int xgridlimit = deviceProp.maxGridSize[0];
  int ygridlimit = deviceProp.maxGridSize[1];
  int zgridlimit = deviceProp.maxGridSize[2];

  int block_count = 0;
  int thread_per_block = 0;

#if GOOGLE_CUDA
  cudaError_t err = cudaOccupancyMaxPotentialBlockSize(
      &block_count, &thread_per_block, func, dynamic_shared_memory_size,
      block_size_limit);
  CHECK_EQ(err, cudaSuccess);
#elif TENSORFLOW_USE_ROCM
  // ROCM TODO re-enable this after hipOccupancyMaxPotentialBlockSize is
  // implemented
  // hipError_t err = hipOccupancyMaxPotentialBlockSize(
  //    &block_count, &thread_per_block, func, dynamic_shared_memory_size,
  //    block_size_limit);
  // CHECK_EQ(err, hipSuccess);

  const int physical_thread_count =
      d.getNumGpuMultiProcessors() * d.maxGpuThreadsPerMultiProcessor();
  thread_per_block = std::min(1024, d.maxGpuThreadsPerBlock());
  block_count =
      std::min(DivUp(physical_thread_count, thread_per_block),
               d.getNumGpuMultiProcessors());
#endif

  int threadsx = std::min({xdim, thread_per_block, xthreadlimit});
  int threadsy =
      std::min({ydim, std::max(thread_per_block / threadsx, 1), ythreadlimit});
  int threadsz =
      std::min({zdim, std::max(thread_per_block / (threadsx * threadsy), 1),
                zthreadlimit});

  int blocksx = std::min({block_count, DivUp(xdim, threadsx), xgridlimit});
  int blocksy = std::min(
      {DivUp(block_count, blocksx), DivUp(ydim, threadsy), ygridlimit});
  int blocksz = std::min({DivUp(block_count, (blocksx * blocksy)),
                          DivUp(zdim, threadsz), zgridlimit});

  config.virtual_thread_count = dim3(xdim, ydim, zdim);
  config.thread_per_block = dim3(threadsx, threadsy, threadsz);
  config.block_count = dim3(blocksx, blocksy, blocksz);
  return config;
}

template <typename DeviceFunc>
inline Cuda2DLaunchConfig GetCuda2DLaunchConfig(
    int xdim, int ydim, const Eigen::GpuDevice& d, DeviceFunc func,
    size_t dynamic_shared_memory_size, int block_size_limit) {
  return GetCuda3DLaunchConfig(xdim, ydim, 1, d, func,
                               dynamic_shared_memory_size, block_size_limit);
}

#if GOOGLE_CUDA
// Returns a raw reference to the current cuda stream.  Required by a
// number of kernel calls (for which StreamInterface* does not work), i.e.
// CUB and certain cublas primitives.
inline const cudaStream_t& GetCudaStream(OpKernelContext* context) {
  const cudaStream_t* ptr = CHECK_NOTNULL(
      reinterpret_cast<const cudaStream_t*>(context->op_device_context()
                                                ->stream()
                                                ->implementation()
                                                ->GpuStreamMemberHack()));
  return *ptr;
}
#endif // GOOGLE_CUDA

namespace detail {
template <typename... Ts, size_t... Is>
std::array<void*, sizeof...(Ts)> GetArrayOfElementPointersImpl(
    std::tuple<Ts...>* tuple, absl::index_sequence<Is...>) {
  return {{&std::get<Is>(*tuple)...}};
}
// Returns an array of void pointers to the elements of the given tuple.
template <typename... Ts>
std::array<void*, sizeof...(Ts)> GetArrayOfElementPointers(
    std::tuple<Ts...>* tuple) {
  return GetArrayOfElementPointersImpl(tuple,
                                       absl::index_sequence_for<Ts...>{});
}

template <bool...>
struct BoolPack;
template <bool... Bs>
using NoneTrue = std::is_same<BoolPack<Bs..., false>, BoolPack<false, Bs...>>;
// Returns whether none of the types in Ts is a reference.
template <typename... Ts>
constexpr bool NoneIsReference() {
  return NoneTrue<(std::is_reference<Ts>::value)...>::value;
}
}  // namespace detail

#if GOOGLE_CUDA
// Launches a CUDA kernel through cudaLaunchKernel with the given arguments.
//
// The kernel parameters 'Ts' must be constructible from the arguments 'Args'.
template <typename... Ts, typename... Args>
Status CudaLaunchKernel(void (*function)(Ts...), dim3 grid_dim, dim3 block_dim,
                        size_t shared_memory_size_bytes, cudaStream_t stream,
                        Args... arguments) {
  static_assert(detail::NoneIsReference<Ts...>(),
                "Kernels with reference arguments have undefined behaviour.");
  // Cast arguments and forward them as an array of pointers.
  auto args_tuple = std::tuple<Ts...>(arguments...);
  auto arg_ptrs = detail::GetArrayOfElementPointers(&args_tuple);
  auto func_ptr = absl::bit_cast<const void*>(function);
  auto result = cudaLaunchKernel(func_ptr, grid_dim, block_dim, arg_ptrs.data(),
                                 shared_memory_size_bytes, stream);
  if (result != cudaSuccess) {
    return errors::Internal(cudaGetErrorString(result));
  }
  return Status::OK();
}
#endif // GOOGLE_CUDA

}  // namespace tensorflow

#endif  // GOOGLE_CUDA || TENSORFLOW_USE_ROCM

#endif  // TENSORFLOW_CORE_UTIL_GPU_LAUNCH_CONFIG_H_

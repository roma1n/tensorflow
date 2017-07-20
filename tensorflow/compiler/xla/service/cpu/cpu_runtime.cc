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

#include "tensorflow/compiler/xla/service/cpu/cpu_runtime.h"

#include <functional>

#include "tensorflow/compiler/xla/map_util.h"
#include "tensorflow/compiler/xla/service/llvm_ir/llvm_util.h"
#include "tensorflow/core/lib/gtl/flatmap.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/macros.h"
#include "tensorflow/core/platform/mutex.h"
#include "tensorflow/core/platform/types.h"

namespace {
tensorflow::gtl::FlatMap<tensorflow::StringPiece, void*>* BuiltinMap() {
  static auto* builtin_map =
      new tensorflow::gtl::FlatMap<tensorflow::StringPiece, void*>;
  return builtin_map;
}

tensorflow::mutex builtin_map_mutex(tensorflow::LINKER_INITIALIZED);
}  // namespace

namespace xla {
namespace cpu {
namespace runtime {

const char* const kEigenMatMulF32SymbolName =
    "__xla_cpu_runtime_EigenMatMulF32";
const char* const kEigenMatMulF64SymbolName =
    "__xla_cpu_runtime_EigenMatMulF64";
const char* const kEigenConvF32SymbolName = "__xla_cpu_runtime_EigenConvF32";
const char* const kEigenSingleThreadedMatMulF32SymbolName =
    "__xla_cpu_runtime_EigenSingleThreadedMatMulF32";
const char* const kEigenSingleThreadedMatMulF64SymbolName =
    "__xla_cpu_runtime_EigenSingleThreadedMatMulF64";
const char* const kEigenSingleThreadedConvF32SymbolName =
    "__xla_cpu_runtime_EigenSingleThreadedConvF32";
const char* const kAcquireInfeedBufferForDequeueSymbolName =
    "__xla_cpu_runtime_AcquireInfeedBufferForDequeue";
const char* const kReleaseInfeedBufferAfterDequeueSymbolName =
    "__xla_cpu_runtime_ReleaseInfeedBufferAfterDequeue";
const char* const kAcquireOutfeedBufferForPopulationSymbolName =
    "__xla_cpu_runtime_AcquireOutfeedBufferForPopulation";
const char* const kReleaseOutfeedBufferAfterPopulationSymbolName =
    "__xla_cpu_runtime_ReleaseOutfeedBufferAfterPopulation";

const char* const kXlaCpuRuntimeSymbolPrefix = "__xla_cpu_runtime_";

XfeedManager* GetXfeedManager() {
  static XfeedManager* manager = new XfeedManager;
  return manager;
}

namespace internal {
Registrar::Registrar(tensorflow::StringPiece name, void* function_pointer,
                     tensorflow::StringPiece base_name) {
  CHECK(function_pointer);
  CHECK_EQ(tensorflow::strings::StrCat(kXlaCpuRuntimeSymbolPrefix, base_name),
           name);

  tensorflow::mutex_lock lock(builtin_map_mutex);
  InsertOrDie(BuiltinMap(), name, function_pointer);
}
}  // namespace internal

void* ResolveSymbol(tensorflow::StringPiece name) {
  CHECK(name.starts_with(kXlaCpuRuntimeSymbolPrefix));

  tensorflow::mutex_lock lock(builtin_map_mutex);
  const auto& builtin_map = *BuiltinMap();
  auto lookup_iterator = builtin_map.find(name);
  return lookup_iterator == builtin_map.end() ? nullptr
                                              : lookup_iterator->second;
}
}  // namespace runtime
}  // namespace cpu
}  // namespace xla

namespace {

tensorflow::string ShapeString(const void* shape_ptr, xla::int32 shape_length) {
  xla::StatusOr<xla::Shape> shape =
      xla::llvm_ir::DecodeSelfDescribingShapeConstant(shape_ptr, shape_length);
  if (shape.ok()) {
    return xla::ShapeUtil::HumanStringWithLayout(shape.ValueOrDie());
  }
  return "<invalid shape>";
}

}  // namespace

void* __xla_cpu_runtime_AcquireInfeedBufferForDequeue(xla::int32 buffer_length,
                                                      const void* shape,
                                                      xla::int32 shape_length) {
  if (VLOG_IS_ON(2)) {
    LOG(INFO) << "AcquireInfeedBufferForDequeue: "
              << ShapeString(shape, shape_length);
  }
  xla::cpu::runtime::XfeedManager* xfeed = xla::cpu::runtime::GetXfeedManager();
  // Wait until there's a buffer to dequeue.
  xla::cpu::runtime::XfeedBuffer* buffer =
      xfeed->infeed()->BlockingDequeueBuffer();
  CHECK_EQ(buffer->length(), buffer_length)
      << "XLA program infeed request buffer size " << buffer_length
      << " did not match the runtime's infed buffer length " << buffer->length()
      << "; program reports desired shape: "
      << ShapeString(shape, shape_length);
  return buffer->data();
}

REGISTER_XLA_CPU_RUNTIME_BUILTIN(AcquireInfeedBufferForDequeue);

void __xla_cpu_runtime_ReleaseInfeedBufferAfterDequeue(
    xla::int32 buffer_length, void* buffer_ptr, const void* shape_ptr,
    xla::int32 shape_length) {
  if (VLOG_IS_ON(2)) {
    LOG(INFO) << "ReleaseInfeedBufferAfterDeque: "
              << ShapeString(shape_ptr, shape_length);
  }
  xla::cpu::runtime::XfeedManager* xfeed = xla::cpu::runtime::GetXfeedManager();
  xla::StatusOr<xla::Shape> shape =
      xla::llvm_ir::DecodeSelfDescribingShapeConstant(shape_ptr, shape_length);
  xfeed->infeed()->ReleaseCurrentBuffer(buffer_length, buffer_ptr,
                                        std::move(shape));
}

REGISTER_XLA_CPU_RUNTIME_BUILTIN(ReleaseInfeedBufferAfterDequeue);

void* __xla_cpu_runtime_AcquireOutfeedBufferForPopulation(
    xla::int32 buffer_length, const void* shape_ptr, xla::int32 shape_length) {
  if (VLOG_IS_ON(2)) {
    LOG(INFO) << "AcquireOutfeedBufferForPopulation: "
              << ShapeString(shape_ptr, shape_length);
  }
  xla::cpu::runtime::XfeedManager* xfeed = xla::cpu::runtime::GetXfeedManager();
  // Wait until there's a buffer to dequeue.
  xla::cpu::runtime::XfeedBuffer* buffer =
      xfeed->outfeed()->BlockingDequeueBuffer();
  CHECK_EQ(buffer->length(), buffer_length)
      << "XLA program outfeed request buffer size " << buffer_length
      << " did not match the runtime's outfeed buffer length "
      << buffer->length() << "; program reports outfed shape: "
      << ShapeString(shape_ptr, shape_length);
  return buffer->data();
}

REGISTER_XLA_CPU_RUNTIME_BUILTIN(AcquireOutfeedBufferForPopulation);

void __xla_cpu_runtime_ReleaseOutfeedBufferAfterPopulation(
    xla::int32 buffer_length, void* buffer_ptr, const void* shape_ptr,
    xla::int32 shape_length) {
  if (VLOG_IS_ON(2)) {
    LOG(INFO) << "ReleaseOutfeedBufferAfterPopulation: "
              << ShapeString(shape_ptr, shape_length);
  }
  xla::cpu::runtime::XfeedManager* xfeed = xla::cpu::runtime::GetXfeedManager();
  xla::StatusOr<xla::Shape> shape =
      xla::llvm_ir::DecodeSelfDescribingShapeConstant(shape_ptr, shape_length);
  xfeed->outfeed()->ReleaseCurrentBuffer(buffer_length, buffer_ptr, shape);
}

REGISTER_XLA_CPU_RUNTIME_BUILTIN(ReleaseOutfeedBufferAfterPopulation);

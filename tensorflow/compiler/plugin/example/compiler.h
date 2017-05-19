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

#ifndef TENSORFLOW_COMPILER_EXAMPLE_COMPILER_H_
#define TENSORFLOW_COMPILER_EXAMPLE_COMPILER_H_

#include <memory>

#include "tensorflow/compiler/xla/service/compiler.h"
#include "tensorflow/compiler/xla/service/executable.h"
#include "tensorflow/compiler/xla/service/hlo_module.h"
#include "tensorflow/compiler/xla/service/hlo_module_config.h"

#include "tensorflow/compiler/plugin/example/platform_id.h"

namespace xla {
namespace exampleplugin {

class ExampleCompiler : public Compiler {
 public:
  ExampleCompiler() {}
  ~ExampleCompiler() override {}

  StatusOr<std::unique_ptr<Executable>> Compile(
      std::unique_ptr<HloModule> hlo_module,
      std::unique_ptr<HloModuleConfig> module_config, HloDumper dump_hlo,
      perftools::gputools::StreamExecutor* stream_exec) override;

  StatusOr<std::vector<std::unique_ptr<Executable>>> Compile(
      std::vector<std::unique_ptr<HloModule>> hlo_module,
      std::vector<std::unique_ptr<HloModuleConfig>> module_config,
      HloDumper dump_hlo,
      std::vector<perftools::gputools::StreamExecutor*> stream_exec) override;

  StatusOr<std::vector<std::unique_ptr<AotCompilationResult>>>
  CompileAheadOfTime(
      std::vector<std::unique_ptr<HloModule>> module,
      std::vector<std::unique_ptr<HloModuleConfig>> module_config,
      HloDumper dump_hlo, const AotCompilationOptions& options) override;

  int64 ShapeSizeBytes(const Shape& shape) const override;

  perftools::gputools::Platform::Id PlatformId() const override;

 private:
  Status RunHloOptimization(HloModule* hlo_module,
                            HloModuleConfig* module_config, HloDumper dump_hlo);

  TF_DISALLOW_COPY_AND_ASSIGN(ExampleCompiler);
};

}  // namespace exampleplugin
}  // namespace xla

#endif  // TENSORFLOW_COMPILER_EXAMPLE_COMPILER_H_

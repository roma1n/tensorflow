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

// Usage: replay_computation some_binary_snapshot_proto*
//
// Replays computations and shows the results on the command line.
//
// some_binary_snapshot_proto is obtained by serializing the HloSnapshot from
// ServiceInterface::SnapshotComputation to disk.
//
// Computations that require arguments can be replayed using fake data by
// passing --use_fake_data on the command line.  If the real data is available
// in the proto and --use_fake_data is false, the real data is used.
//
// The output format is:
//
// file_path: computation_name :: type:literal_str

#include <stdio.h>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "tensorflow/compiler/xla/client/client.h"
#include "tensorflow/compiler/xla/client/client_library.h"
#include "tensorflow/compiler/xla/client/global_data.h"
#include "tensorflow/compiler/xla/client/lib/testing.h"
#include "tensorflow/compiler/xla/client/local_client.h"
#include "tensorflow/compiler/xla/execution_options_util.h"
#include "tensorflow/compiler/xla/literal_util.h"
#include "tensorflow/compiler/xla/service/gpu/infeed_manager.h"
#include "tensorflow/compiler/xla/service/hlo.pb.h"
#include "tensorflow/compiler/xla/shape_util.h"
#include "tensorflow/compiler/xla/status_macros.h"
#include "tensorflow/compiler/xla/statusor.h"
#include "tensorflow/compiler/xla/tests/test_utils.h"
#include "tensorflow/compiler/xla/types.h"
#include "tensorflow/compiler/xla/xla_data.pb.h"
#include "tensorflow/core/lib/core/threadpool.h"
#include "tensorflow/core/lib/gtl/array_slice.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/platform/init_main.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/util/command_line_flags.h"

namespace xla {
namespace tools {
namespace {

// Command-line opts to this tool.  See main() for descriptions of these
// fields.
struct Options {
  string fake_infeed_shape;
  bool generate_fake_infeed = false;
  bool use_fake_data = false;
  bool print_result = true;
  int num_runs = 1;
};

// Invokes the given computation passing arbitrary data for every (unbound)
// parameter if use_fake_data, Otherwise use recorded data if available.
//
// Similarly, infeeds fake data of shape fake_infeed_shape if it is provided.
// If generate_fake_infeed is true, the required infeed shape is derived from
// the computation and then used to provide a fake infeed shape.
//
// If neither generate_fake_infeed is true nor a fake_infeed_shape is provided,
// no infeed is performed.
StatusOr<Literal> ReplayComputation(const HloSnapshot& module,
                                    LocalClient* client, const Options& opts) {
  XlaComputation computation(module.hlo().hlo_module());

  // Build the `argument_ptrs` vector, which contains ShapedBuffer*s to our
  // arguments.  This is a bit involved, because we may have to convert from
  // GlobalData to ShapedBuffer*, and we have to manage the lifetime of all our
  // objects.
  std::vector<ScopedShapedBuffer> scoped_shaped_buffer_arguments;
  std::vector<std::unique_ptr<GlobalData>> global_data_arguments;
  std::vector<const ShapedBuffer*> argument_ptrs;
  if (opts.use_fake_data) {
    global_data_arguments = MakeFakeArgumentsOrDie(computation, client);
    for (const auto& data : global_data_arguments) {
      argument_ptrs.push_back(
          client->GlobalDataToShapedBuffer(data->handle(), /*device_ordinal=*/0)
              .ValueOrDie());
    }
  } else {  // use recorded data if available
    for (const auto& proto : module.arguments()) {
      TF_ASSIGN_OR_RETURN(std::unique_ptr<xla::Literal> literal,
                          Literal::CreateFromProto(proto));
      TF_ASSIGN_OR_RETURN(
          ScopedShapedBuffer data,
          client->LiteralToShapedBuffer(*literal, /*device_ordinal=*/0));
      scoped_shaped_buffer_arguments.push_back(std::move(data));
    }
    for (const auto& argument : scoped_shaped_buffer_arguments) {
      argument_ptrs.push_back(&argument);
    }
  }

  bool provide_infeed = false;
  Shape infeed_shape;
  if (!opts.fake_infeed_shape.empty()) {
    StatusOr<Shape> shape_status =
        ShapeUtil::ParseShapeString(opts.fake_infeed_shape);
    TF_CHECK_OK(shape_status.status());
    infeed_shape = std::move(shape_status).ValueOrDie();
    provide_infeed = true;
  } else if (opts.generate_fake_infeed) {
    for (const auto& comp : computation.proto().computations()) {
      for (const auto& instruction : comp.instructions()) {
        if (instruction.opcode() == HloOpcodeString(HloOpcode::kInfeed)) {
          CHECK(!provide_infeed)
              << "--generate_fake_infeed only works if the model has 0 or 1 "
                 "infeed ops, but this one has >= 2.";
          provide_infeed = true;
          infeed_shape = instruction.shape();
          LOG(INFO) << "Generating fake infeed shape for inferred shape: "
                    << ShapeUtil::HumanString(infeed_shape);
        }
      }
    }
  }
  // We only instantiate the thread pool if the user has requested that a
  // concurrent infeed occur via the fake_infeed_shape, or when
  // --generate_fake_infeed is passed and there exists an infeed operation in
  // the HloSnapshot.
  tensorflow::gtl::optional<tensorflow::thread::ThreadPool> pool;
  std::unique_ptr<Literal> data;
  if (provide_infeed) {
    data = std::move(MakeFakeLiteral(infeed_shape)).ValueOrDie();
  }
  auto transfer_infeed = [&data, client]() {
    TF_CHECK_OK(client->TransferToInfeed(*data));
  };
  if (provide_infeed) {
    pool.emplace(tensorflow::Env::Default(), "infeed",
                 /*num_threads=*/1);
    pool->Schedule([transfer_infeed]() {
      // There may be several infeed buffers needed, however we don't know how
      // many. If we proactively transfer too many infeed buffers, we may run
      // out of memory. If we transfer too few infeed buffers, the program will
      // hang. Therefore, we register a callback that is called when the infeed
      // becomes empty, and in this callback we will transfer another fake
      // infeed.
      auto infeed_manager = xla::gpu::GetOrCreateInfeedManager();
      infeed_manager->RegisterOnEmptyCallback(transfer_infeed);
      transfer_infeed();
    });
  }

  std::vector<const Shape*> argument_layouts;
  for (const auto& param : computation.proto().program_shape().parameters()) {
    argument_layouts.push_back(&param);
  }
  std::unique_ptr<LocalExecutable> executable =
      client->Compile(computation, argument_layouts, ExecutableBuildOptions())
          .ValueOrDie();

  // Run the computation num_runs times, and return the result from the last
  // execution.
  StreamExecutorMemoryAllocator allocator(
      client->platform(),
      {client->platform()->ExecutorForDevice(0).ValueOrDie()});
  tensorflow::gtl::optional<ScopedShapedBuffer> result;
  for (int i = 0; i < opts.num_runs; ++i) {
    ExecutionProfile profile;
    ExecutableRunOptions run_options;
    run_options.set_execution_profile(&profile);
    run_options.set_allocator(&allocator);

    TF_ASSIGN_OR_RETURN(result, executable->Run(argument_ptrs, run_options));
    LOG(INFO) << "Execution took "
              << static_cast<double>(profile.compute_time_ns()) / 1e9 << "s";
  }

  // Check that --num_runs > 0, otherwise *result below will fail with an
  // unhelpful error (because the loop didn't run any iterations).
  CHECK_GT(opts.num_runs, 0) << "--num_runs must be > 0";
  TF_ASSIGN_OR_RETURN(std::unique_ptr<Literal> result_literal,
                      client->ShapedBufferToLiteral(*result));
  return std::move(*result_literal);
}

int RealMain(tensorflow::gtl::ArraySlice<char*> args, const Options& opts) {
  LocalClient* client = ClientLibrary::LocalClientOrDie();
  tensorflow::Env* env = tensorflow::Env::Default();
  int exit_status = EXIT_SUCCESS;
  for (char* arg : args) {
    HloSnapshot snapshot;
    auto status = tensorflow::ReadBinaryProto(env, arg, &snapshot);
    if (!status.ok()) {
      fprintf(stderr, "%s: is not HloSnapshot. Trying HloProto.\n", arg);
      status = tensorflow::ReadBinaryProto(env, arg, snapshot.mutable_hlo());
      if (!status.ok()) {
        fprintf(stderr, "%s: is not HloSnapshot or HloProto: %s.\n", arg,
                status.ToString().c_str());
        continue;
      }
      CHECK(opts.use_fake_data)
          << "HloProto input must be handled with --use_fake_data";
    }

    StatusOr<Literal> result_status = ReplayComputation(snapshot, client, opts);
    if (!result_status.ok()) {
      fprintf(stderr, "%s: error: %s\n", arg,
              result_status.status().ToString().c_str());
      exit_status = EXIT_FAILURE;
      continue;
    }

    if (opts.print_result) {
      Literal result = std::move(result_status).ValueOrDie();
      fprintf(stdout, "%s: %s :: %s:%s\n", arg,
              snapshot.hlo().hlo_module().name().c_str(),
              ShapeUtil::HumanString(result.shape()).c_str(),
              result.ToString().c_str());
      if (snapshot.has_result()) {
        std::unique_ptr<Literal> literal =
            Literal::CreateFromProto(snapshot.result()).ConsumeValueOrDie();
        fprintf(stdout, "was %s:%s\n",
                ShapeUtil::HumanString(snapshot.result().shape()).c_str(),
                literal->ToString().c_str());
      }
    }
  }

  ClientLibrary::DestroyLocalInstances();
  return exit_status;
}

}  // namespace
}  // namespace tools
}  // namespace xla

int main(int argc, char** argv) {
  xla::tools::Options opts;
  const std::vector<tensorflow::Flag> flag_list = {
      tensorflow::Flag("use_fake_data", &opts.use_fake_data,
                       "Replay computation using fake data"),
      tensorflow::Flag("print_result", &opts.print_result,
                       "Print the result of the computation to stdout"),
      tensorflow::Flag("num_runs", &opts.num_runs,
                       "Number of times to run each computation"),
      tensorflow::Flag("fake_infeed_shape", &opts.fake_infeed_shape,
                       "Shape of fake data to construct for (infinite) infeed"),
      tensorflow::Flag("generate_fake_infeed", &opts.generate_fake_infeed,
                       "Whether a fake infeed shape should be generated "
                       "derived from the computation"),
  };
  xla::string usage = tensorflow::Flags::Usage(argv[0], flag_list);
  bool parse_ok = tensorflow::Flags::Parse(&argc, argv, flag_list);
  tensorflow::port::InitMain(argv[0], &argc, &argv);
  if (argc < 2 || !parse_ok) {
    LOG(QFATAL) << usage;
  }

  tensorflow::gtl::ArraySlice<char*> args(argv, argc);
  args.pop_front();  // Pop off the binary name, argv[0]
  return xla::tools::RealMain(args, opts);
}

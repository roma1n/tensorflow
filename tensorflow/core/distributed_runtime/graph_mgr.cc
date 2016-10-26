/* Copyright 2016 The TensorFlow Authors. All Rights Reserved.

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

#include "tensorflow/core/distributed_runtime/graph_mgr.h"

#include <vector>

#include "tensorflow/core/common_runtime/constant_folding.h"
#include "tensorflow/core/common_runtime/device.h"
#include "tensorflow/core/common_runtime/device_mgr.h"
#include "tensorflow/core/common_runtime/function.h"
#include "tensorflow/core/common_runtime/graph_optimizer.h"
#include "tensorflow/core/common_runtime/memory_types.h"
#include "tensorflow/core/common_runtime/process_util.h"
#include "tensorflow/core/common_runtime/step_stats_collector.h"
#include "tensorflow/core/distributed_runtime/rendezvous_mgr_interface.h"
#include "tensorflow/core/framework/cancellation.h"
#include "tensorflow/core/framework/log_memory.h"
#include "tensorflow/core/framework/node_def_util.h"
#include "tensorflow/core/graph/graph.h"
#include "tensorflow/core/graph/graph_constructor.h"
#include "tensorflow/core/graph/graph_partition.h"
#include "tensorflow/core/graph/validate.h"
#include "tensorflow/core/lib/core/errors.h"
#include "tensorflow/core/lib/strings/stringprintf.h"
#include "tensorflow/core/platform/env.h"
#include "tensorflow/core/platform/logging.h"
#include "tensorflow/core/platform/mutex.h"
#include "tensorflow/core/platform/types.h"
#include "tensorflow/core/protobuf/worker.pb.h"

namespace tensorflow {

GraphMgr::GraphMgr(const WorkerEnv* worker_env)
    : worker_env_(worker_env), table_(5) {}

GraphMgr::~GraphMgr() {
  for (auto p : table_) p.second->Unref();
}

GraphMgr::Item::~Item() {
  for (const auto& unit : this->units) {
    CHECK_NOTNULL(unit.device);
    delete unit.root;
    delete unit.lib;
    unit.device->op_segment()->RemoveHold(this->session);
  }
  delete this->lib_def;
}

// NOTE: node->device_name() is not set by GraphConstructor.  We
// expects that NodeDef in GraphDef given to workers fully specifies
// device names.
static string SplitByDevice(const Node* node) {
  return node->assigned_device_name();
}

// Validates "gdef" device specifications.
static Status ValidateGraphDefForDevices(const GraphDef& gdef) {
  DeviceNameUtils::ParsedName parsed;
  for (const auto& ndef : gdef.node()) {
    if (!DeviceNameUtils::ParseFullName(ndef.device(), &parsed)) {
      return errors::InvalidArgument("Missing device name in: ",
                                     SummarizeNodeDef(ndef));
    }
  }
  return Status::OK();
}

// Creates executors given a graph definition "gdef" of a "session".
// If a node in "gdef" is shared by other graphs in "session", the
// same op kernel is reused. E.g., typically a params node is shared
// by multiple graphs in a session.
//
// If "gdef" is assigned to multiple devices, extra nodes (e.g.,
// send/recv nodes) maybe added. The extra nodes' name are generated
// by calling "new_name(old_name)".
//
// "executors" are filled with one executor per device if success and
// the caller takes the ownership of returned executors.
Status GraphMgr::InitItem(const string& session, const GraphDef& gdef,
                          const GraphOptions& graph_options, Item* item) {
  item->session = session;
  item->lib_def =
      new FunctionLibraryDefinition(OpRegistry::Global(), gdef.library());

  TF_RETURN_IF_ERROR(ValidateGraphDefForDevices(gdef));

  if (gdef.versions().producer() >= 5) {
    // Validate the graph: we assume that merging two valid graphs
    // should maintain graph validity.
    TF_RETURN_IF_ERROR(graph::ValidateGraphDef(gdef, *item->lib_def));
  }

  // Constructs the graph out of "gdef".
  Graph graph(item->lib_def);
  GraphConstructorOptions opts;
  opts.allow_internal_ops = true;
  opts.expect_device_spec = true;
  TF_RETURN_IF_ERROR(ConvertGraphDefToGraph(opts, gdef, &graph));

  // Splits "graph" into multiple subgraphs by device names.
  std::unordered_map<string, GraphDef> partitions;
  PartitionOptions popts;
  popts.node_to_loc = SplitByDevice;
  popts.new_name = [this](const string& prefix) {
    mutex_lock l(mu_);
    return strings::StrCat(prefix, "_G", next_id_++);
  };
  popts.get_incarnation = [this](const string& name) -> int64 {
    Device* device = nullptr;
    Status s = worker_env_->device_mgr->LookupDevice(name, &device);
    if (s.ok()) {
      return device->attributes().incarnation();
    } else {
      return PartitionOptions::kIllegalIncarnation;
    }
  };
  popts.control_flow_added = true;
  popts.scheduling_for_recvs = graph_options.enable_recv_scheduling();
  TF_RETURN_IF_ERROR(Partition(popts, &graph, &partitions));
  if (popts.scheduling_for_recvs) {
    TF_RETURN_IF_ERROR(AddControlEdges(popts, &partitions));
  }

  LocalExecutorParams params;

  Status s;
  item->units.reserve(partitions.size());
  const auto& optimizer_opts = graph_options.optimizer_options();
  GraphOptimizer optimizer(optimizer_opts);
  for (auto&& p : partitions) {
    const string& device_name = p.first;
    GraphDef* def = &p.second;
    item->units.resize(item->units.size() + 1);
    ExecutionUnit* unit = &(item->units.back());

    // Find the device.
    s = worker_env_->device_mgr->LookupDevice(device_name, &unit->device);
    if (!s.ok()) break;

    // Construct the subgraph.
    Graph* subgraph = new Graph(item->lib_def);
    // Give the device an opportunity to rewrite its subgraph.
    unit->device->MaybeRewriteGraph(gdef.library(), def);
    s = ConvertGraphDefToGraph(opts, *def, subgraph);
    if (!s.ok()) {
      delete subgraph;
      break;
    }
    // Top-level nodes in the graph uses the op segment to cache
    // kernels. Therefore, as long as the executor is alive, we need
    // to ensure the kernels cached for the session are alive.
    auto opseg = unit->device->op_segment();
    opseg->AddHold(session);

    // Function library runtime.
    unit->lib = NewFunctionLibraryRuntime(
        worker_env_->device_mgr, worker_env_->env, unit->device,
        def->versions().producer(), item->lib_def,
        graph_options.optimizer_options());

    // Construct the root executor for the subgraph.
    params.device = unit->device;
    auto lib = unit->lib;
    params.function_library = lib;
    params.create_kernel = [session, lib, opseg](const NodeDef& ndef,
                                                 OpKernel** kernel) {
      // Caches the kernel only if the node is stateful.
      if (!lib->IsStateful(ndef.op())) {
        return lib->CreateKernel(ndef, kernel);
      }
      auto create_fn = [lib, &ndef](OpKernel** kernel) {
        return lib->CreateKernel(ndef, kernel);
      };
      // Kernels created for subgraph nodes need to be cached.  On
      // cache miss, create_fn() is invoked to create a kernel based
      // on the function library here + global op registry.
      return opseg->FindOrCreate(session, ndef.name(), kernel, create_fn);
    };
    params.delete_kernel = [lib](OpKernel* kernel) {
      // If the node is stateful, opseg owns it. Otherwise, delete it.
      if (kernel && !lib->IsStateful(kernel->type_string())) {
        delete kernel;
      }
    };

    optimizer.Optimize(lib, worker_env_->env, params.device, &subgraph);
    s = EnsureMemoryTypes(DeviceType(unit->device->device_type()),
                          unit->device->name(), subgraph);
    if (!s.ok()) {
      delete subgraph;
      break;
    }
    s = NewLocalExecutor(params, subgraph, &unit->root);
    if (!s.ok()) {
      break;
    }
    unit->graph = subgraph;
    unit->build_cost_model = graph_options.build_cost_model();
    if (unit->build_cost_model > 0) {
      skip_cost_models_ = false;
    }
  }
  return s;
}

Status GraphMgr::Register(const string& session, const GraphDef& gdef,
                          const GraphOptions& graph_options, string* handle) {
  Item* item = new Item;
  Status s = InitItem(session, gdef, graph_options, item);
  if (!s.ok()) {
    item->Unref();
    return s;
  }

  // Inserts one item into table_.
  {
    mutex_lock l(mu_);
    *handle = strings::Printf("%016llx", ++next_id_);
    item->handle = *handle;
    CHECK(table_.insert({*handle, item}).second);
  }
  return Status::OK();
}

Status GraphMgr::Deregister(const string& handle) {
  Item* item = nullptr;
  // Removes one item from table_.
  {
    mutex_lock l(mu_);
    auto iter = table_.find(handle);
    if (iter == table_.end()) {
      return errors::Aborted("Graph handle is not found: ", handle,
                             ". Possibly, this worker just restarted.");
    }
    item = iter->second;
    table_.erase(iter);
  }
  item->Unref();
  return Status::OK();
}

Status GraphMgr::DeregisterAll() {
  std::vector<Item*> items;
  // Removes all items from table_.
  {
    mutex_lock l(mu_);
    for (const auto& entry : table_) {
      items.push_back(entry.second);
    }
    table_.clear();
  }
  for (auto item : items) {
    item->Unref();
  }
  return Status::OK();
}

Status GraphMgr::SendInputsToRendezvous(Rendezvous* rendezvous,
                                        const NamedTensors& in) {
  Rendezvous::ParsedKey parsed;
  for (const auto& p : in) {
    const string& key = p.first;
    const Tensor& val = p.second;

    Status s = Rendezvous::ParseKey(key, &parsed);
    if (s.ok()) {
      s = rendezvous->Send(parsed, Rendezvous::Args(), val, false);
    }
    if (!s.ok()) {
      return s;
    }
  }
  return Status::OK();
}

Status GraphMgr::RecvOutputsFromRendezvous(Rendezvous* rendezvous,
                                           NamedTensors* out) {
  // Receives values requested by the caller.
  Rendezvous::ParsedKey parsed;
  for (auto& p : *out) {
    const string& key = p.first;
    Tensor* val = &p.second;
    bool is_dead = false;
    Status s = Rendezvous::ParseKey(key, &parsed);
    if (s.ok()) {
      s = rendezvous->Recv(parsed, Rendezvous::Args(), val, &is_dead);
    }
    if (is_dead) {
      s = errors::InvalidArgument("The tensor returned for ", key,
                                  " was not valid.");
    }
    if (!s.ok()) return s;
  }
  return Status::OK();
}

Status GraphMgr::SendInputs(const int64 step_id, const NamedTensors& in) {
  Rendezvous* rendezvous = worker_env_->rendezvous_mgr->Find(step_id);
  Status s = SendInputsToRendezvous(rendezvous, in);
  rendezvous->Unref();
  return s;
}

Status GraphMgr::RecvOutputs(const int64 step_id, NamedTensors* out) {
  Rendezvous* rendezvous = worker_env_->rendezvous_mgr->Find(step_id);
  Status s = RecvOutputsFromRendezvous(rendezvous, out);
  rendezvous->Unref();
  return s;
}

void GraphMgr::ExecuteAsync(const string& handle, const int64 step_id,
                            const ExecutorOpts& opts,
                            StepStatsCollector* collector,
                            CostGraphDef* cost_graph,
                            CancellationManager* cancellation_manager,
                            const NamedTensors& in, StatusCallback done) {
  // Lookup an item. Holds one ref while executing.
  Item* item = nullptr;
  {
    mutex_lock l(mu_);
    auto iter = table_.find(handle);
    if (iter != table_.end()) {
      item = iter->second;
      item->Ref();
    }
  }

  if (item == nullptr) {
    done(errors::Aborted("Graph handle is not found: ", handle));
    return;
  }

  Rendezvous* rendezvous = worker_env_->rendezvous_mgr->Find(step_id);

  // Sends values specified by the caller.
  Status s = SendInputsToRendezvous(rendezvous, in);
  if (!s.ok()) {
    done(s);
    item->Unref();
    rendezvous->Unref();
    return;
  }

  StartParallelExecutors(handle, item, rendezvous, collector, cost_graph,
                         cancellation_manager,
                         [this, item, rendezvous, done](const Status& s) {
                           done(s);
                           rendezvous->Unref();
                           item->Unref();
                         });
}

void GraphMgr::StartParallelExecutors(const string& handle, Item* item,
                                      Rendezvous* rendezvous,
                                      StepStatsCollector* collector,
                                      CostGraphDef* cost_graph,
                                      CancellationManager* cancellation_manager,
                                      StatusCallback done) {
  const int num_units = item->units.size();
  CHECK_GE(num_units, 1);
  ResourceMgr* step_resource_manager = new ResourceMgr;
  // NOTE: Transfer one ref of rendezvous and item.
  ExecutorBarrier* barrier = new ExecutorBarrier(
      num_units, rendezvous, [this, item, collector, cost_graph,
                              step_resource_manager, done](const Status& s) {
        BuildCostModel(item, collector, cost_graph);
        done(s);
        delete step_resource_manager;
      });
  Executor::Args args;
  {
    mutex_lock l(mu_);
    args.step_id = ++next_id_;
  }
  args.rendezvous = rendezvous;
  args.cancellation_manager = cancellation_manager;
  args.stats_collector = collector;
  args.step_resource_manager = step_resource_manager;
  if (LogMemory::IsEnabled()) {
    LogMemory::RecordStep(args.step_id, handle);
  }
  thread::ThreadPool* pool = worker_env_->compute_pool;
  using namespace std::placeholders;
  // Line below is equivalent to this code, but does one less indirect call:
  //  args.runner = [pool](std::function<void()> fn) { pool->Schedule(fn); };
  args.runner = std::bind(&thread::ThreadPool::Schedule, pool, _1);
  for (const auto& unit : item->units) {
    unit.root->RunAsync(args, barrier->Get());
  }
}

void GraphMgr::BuildCostModel(Item* item, StepStatsCollector* collector,
                              CostGraphDef* cost_graph) {
  if (collector && cost_graph && !skip_cost_models_) {
    // Build the cost model
    std::unordered_map<string, const Graph*> device_to_graph;
    for (const auto& unit : item->units) {
      if (unit.build_cost_model > 0) {
        device_to_graph[unit.device->name()] = unit.graph;
      }
    }
    collector->BuildCostModel(&cost_model_manager_, device_to_graph);
    for (const auto& device_and_graph : device_to_graph) {
      cost_model_manager_.AddToCostGraphDef(device_and_graph.second,
                                            cost_graph);
    }
  }
}

}  // end namespace tensorflow

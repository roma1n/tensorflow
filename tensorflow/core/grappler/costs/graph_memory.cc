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

#include "tensorflow/core/grappler/costs/graph_memory.h"
#include <list>
#include "tensorflow/core/framework/allocation_description.pb.h"
#include "tensorflow/core/framework/attr_value.pb.h"
#include "tensorflow/core/framework/node_def.pb.h"
#include "tensorflow/core/framework/step_stats.pb.h"
#include "tensorflow/core/framework/tensor_description.pb.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/framework/tensor_shape.pb.h"
#include "tensorflow/core/grappler/clusters/virtual_cluster.h"
#include "tensorflow/core/grappler/costs/graph_properties.h"
#include "tensorflow/core/grappler/utils.h"

namespace tensorflow {
namespace grappler {

Status GraphMemory::InferStatically(
    const std::unordered_map<string, DeviceProperties>& devices) {
  VirtualCluster cluster(devices);
  TF_RETURN_IF_ERROR(cluster.Provision());
  TF_RETURN_IF_ERROR(cluster.Initialize(item_));
  RunMetadata metadata;
  Status s = cluster.Run(item_.graph, item_.feed, item_.fetch, &metadata);
  // The virtual cluster returns the RESOURCE_EXHAUSTED error when it detects
  // that the model would run out of memory. We still get the metadata we need
  // out of the simulation, so we just ignore this error.
  if (!s.ok() && s.code() != error::RESOURCE_EXHAUSTED) {
    return s;
  }
  InferFromTrace(metadata.step_stats());
  return Status::OK();
}

Status GraphMemory::InferDynamically(Cluster* cluster) {
  if (!cluster->DetailedStatsEnabled()) {
    return errors::Unavailable("Detailed stats collection must be enabled");
  }

  TF_RETURN_IF_ERROR(cluster->Initialize(item_));
  RunMetadata metadata;
  TF_RETURN_IF_ERROR(
      cluster->Run(item_.graph, item_.feed, item_.fetch, &metadata));
  InferFromTrace(metadata.step_stats());
  return Status::OK();
}

int64 GraphMemory::GetWorstCaseMemoryUsage() const {
  int64 worst_case = -1;
  for (const auto& peak_usage : peak_usage_) {
    worst_case = std::max(worst_case, peak_usage.second.used_memory);
  }
  return worst_case;
}

void GraphMemory::InferMemUsageForNodes(
    const std::vector<const NodeDef*>& nodes, GraphProperties* properties,
    int64* worst_case_memory_usage, int64* best_case_memory_usage) const {
  // TODO(bsteiner) refine this: we should consider the multidevice case.
  *worst_case_memory_usage = 0;
  *best_case_memory_usage = 0;
  for (const auto& node : item_.graph.node()) {
    // Estimate the memory required to store the tensors generated by the node.
    std::vector<OpInfo::TensorProperties> outputs =
        properties->GetOutputProperties(node.name());
    int64 node_memory_usage = InferMemUsageForNeighbors(outputs);

    // Worst case memory usage corresponds to the case where all the nodes are
    // alive.
    *worst_case_memory_usage += node_memory_usage;

    // Estimate the memory required to store the input tensors needed by the
    // node.
    std::vector<OpInfo::TensorProperties> inputs =
        properties->GetInputProperties(node.name());
    node_memory_usage += InferMemUsageForNeighbors(inputs);

    *best_case_memory_usage =
        std::max(*best_case_memory_usage, node_memory_usage);
  }
}

int64 GraphMemory::InferMemUsageForNeighbors(
    const std::vector<OpInfo::TensorProperties>& props) const {
  int64 neighbors_memory_usage = 0;
  for (const auto& prop : props) {
    DataType dtype = prop.dtype();
    int size = DataTypeSize(dtype);
    TensorShapeProto shape = prop.shape();
    if (shape.unknown_rank()) {
      // Can't infer the size if the rank is unknown, just skip.
      continue;
    }
    // If one of the dimensions is unknown statically, assume it's one.
    for (int i = 0; i < shape.dim_size(); ++i) {
      if (shape.dim(i).size() < 0) {
        shape.mutable_dim(i)->set_size(1);
      }
    }
    int num_elems = TensorShape(shape).num_elements();
    neighbors_memory_usage += num_elems * size;
  }
  return neighbors_memory_usage;
}

static GraphMemory::LiveTensor* FindOrCreateLiveTensor(
    const string& node_name, int output_id,
    std::unordered_map<string, GraphMemory::LiveTensor*>* live_tensors,
    std::list<GraphMemory::LiveTensor>* device_tensors) {
  string name = strings::StrCat(node_name, ":", output_id);
  GraphMemory::LiveTensor* live;
  auto it = live_tensors->find(name);
  if (it == live_tensors->end()) {
    GraphMemory::LiveTensor temp;
    temp.node = node_name;
    temp.output_id = output_id;
    temp.allocation_time = 0;
    temp.deallocation_time = 0;
    device_tensors->push_front(temp);
    live = &device_tensors->front();
    (*live_tensors)[name] = live;
  } else {
    live = it->second;
  }
  return live;
}

namespace {
struct Event {
  int64 timestamp;
  bool allocated;
  const GraphMemory::LiveTensor* tensor;

  bool operator<(const Event& other) const {
    return timestamp < other.timestamp;
  }
};
}  // namespace

void GraphMemory::InferFromTrace(const StepStats& timeline) {
  std::unordered_map<string, string> node_placement;
  for (const auto& dev_stats : timeline.dev_stats()) {
    for (const auto& node_stats : dev_stats.node_stats()) {
      node_placement[node_stats.node_name()] = dev_stats.device();
    }
  }

  std::unordered_map<string, LiveTensor*> live_tensors;
  std::unordered_map<string, std::list<LiveTensor>> live_tensors_per_device;

  NodeMap node_map(&item_.graph);
  for (const auto& dev_stats : timeline.dev_stats()) {
    const string& device_name = dev_stats.device();
    const bool is_gpu = (device_name.find("GPU:") || device_name.find("gpu:"));
    std::list<LiveTensor>& device_tensors =
        live_tensors_per_device[dev_stats.device()];
    for (const auto& node_stats : dev_stats.node_stats()) {
      for (int i = 0; i < node_stats.output_size(); ++i) {
        const auto& output = node_stats.output(i);

        LiveTensor* live = FindOrCreateLiveTensor(
            node_stats.node_name(), i, &live_tensors, &device_tensors);
        live->memory_used = output.tensor_description()
                                .allocation_description()
                                .allocated_bytes();

        // Allocations typically take place at the very beginning of the op
        // execution.
        live->allocation_time =
            Costs::MicroSeconds(node_stats.all_start_micros());
        // Add one nanosecond to the completion time of the ops to account for
        // TF overhead that slightly delays deallocations.
        live->deallocation_time = std::max<Costs::Duration>(
            live->deallocation_time,
            Costs::NanoSeconds(1) +
                Costs::MicroSeconds(node_stats.all_start_micros() +
                                    node_stats.op_end_rel_micros()));
      }

      const NodeDef* node = node_map.GetNode(node_stats.node_name());
      if (!node) {
        // Skip nodes inserted by TF since they don't exist in the original
        // graph (e.g _Send/_Recv nodes).
        continue;
      }
      std::unordered_set<int> swapped_inputs;
      if (is_gpu) {
        auto it = node->attr().find("_swap_to_host");
        if (it != node->attr().end()) {
          const AttrValue& val = it->second;
          for (int port_id : val.list().i()) {
            swapped_inputs.insert(port_id);
          }
        }
      }
      for (int i = 0; i < node->input_size(); ++i) {
        if (swapped_inputs.find(i) != swapped_inputs.end()) {
          // The memory of swapped inputs will be released as early as possible:
          // therefore ignore this input when determining the deallocation time
          // of the tensor.
          continue;
        }
        const string& input = node->input(i);
        int position;
        string input_node = ParseNodeName(input, &position);
        if (position < 0) {
          // Skip control dependencies
          continue;
        }
        LiveTensor* live = FindOrCreateLiveTensor(
            input_node, position, &live_tensors,
            &live_tensors_per_device[node_placement[input_node]]);
        live->deallocation_time = std::max<Costs::Duration>(
            live->deallocation_time,
            Costs::NanoSeconds(1) +
                Costs::MicroSeconds(node_stats.all_start_micros() +
                                    node_stats.op_end_rel_micros()));
      }
    }
  }

  for (const auto& live_per_device : live_tensors_per_device) {
    std::vector<Event> events;
    events.reserve(2 * live_per_device.second.size());
    for (const auto& live : live_per_device.second) {
      events.push_back(Event{live.allocation_time.count(), true, &live});
      events.push_back(Event{live.deallocation_time.count(), false, &live});
    }
    std::stable_sort(events.begin(), events.end());
    size_t peak = 0;
    std::set<const LiveTensor*> live_at_peak;
    size_t current = 0;
    std::set<const LiveTensor*> currently_live;
    for (int i = 0; i < events.size(); ++i) {
      const auto& event = events[i];

      if (event.allocated) {
        VLOG(1) << "At time " << event.timestamp << " allocated "
                << event.tensor->memory_used << " for tensor "
                << event.tensor->node << ":" << event.tensor->output_id;
        current += event.tensor->memory_used;
        currently_live.insert(event.tensor);
      } else {
        VLOG(1) << "At time " << event.timestamp << " deallocated "
                << event.tensor->memory_used << " for tensor "
                << event.tensor->node << ":" << event.tensor->output_id;
        current -= event.tensor->memory_used;
        currently_live.erase(event.tensor);
      }
      if (i + 1 == events.size() ||
          event.timestamp != events[i + 1].timestamp) {
        if (current > peak) {
          peak = current;
          live_at_peak = currently_live;
        }
      }
    }
    MemoryUsage& peak_mem_usage = peak_usage_[live_per_device.first];
    peak_mem_usage.used_memory = peak;
    peak_mem_usage.live_tensors.clear();
    peak_mem_usage.live_tensors.reserve(live_at_peak.size());
    for (const auto& live : live_at_peak) {
      peak_mem_usage.live_tensors.push_back(*live);
    }
  }
}

}  // end namespace grappler
}  // end namespace tensorflow

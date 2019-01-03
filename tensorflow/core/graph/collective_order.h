/* Copyright 2018 The TensorFlow Authors. All Rights Reserved.

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
#ifndef TENSORFLOW_CORE_GRAPH_COLLECTIVE_ORDER_H_
#define TENSORFLOW_CORE_GRAPH_COLLECTIVE_ORDER_H_

#include "tensorflow/core/graph/graph.h"

namespace tensorflow {

// Introduces control edges between potentially concurrent CollectiveOps to make
// their execution order deterministic. This may be used to execute collectives
// in the same order across all workers in a distributed execution, if all
// workers are executing the same graph.
Status OrderCollectives(Graph* graph);

}  // namespace tensorflow

#endif  // TENSORFLOW_CORE_GRAPH_COLLECTIVE_ORDER_H_

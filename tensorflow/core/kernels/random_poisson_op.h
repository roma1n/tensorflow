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

#ifndef TENSORFLOW_KERNELS_RANDOM_POISSON_OP_H_
#define TENSORFLOW_KERNELS_RANDOM_POISSON_OP_H_

namespace tensorflow {

namespace functor {

// Generic helper functor for the Random Poisson Op.
template <typename Device, typename T>
struct PoissonFunctor;

}  // namespace functor

}  // namespace tensorflow

#endif  // TENSORFLOW_KERNELS_RANDOM_POISSON_OP_H_

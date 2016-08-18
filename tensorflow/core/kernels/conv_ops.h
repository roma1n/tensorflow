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

#ifndef TENSORFLOW_KERNELS_CONV_OPS_H_
#define TENSORFLOW_KERNELS_CONV_OPS_H_

#include "third_party/eigen3/unsupported/Eigen/CXX11/Tensor"
#include "tensorflow/core/util/tensor_format.h"

#if GOOGLE_CUDA
#include "tensorflow/core/kernels/conv_ops_gpu.h"
#include "tensorflow/core/platform/stream_executor.h"
#endif  // GOOGLE_CUDA

namespace tensorflow {

// Forward declaration.
class OpKernelContext;

template <typename Device, typename T>
class LaunchConv2DOp {
 public:
  void launch(OpKernelContext* ctx, bool use_cudnn, bool cudnn_use_autotune,
              const Tensor& input, const Tensor& filter, int row_stride,
              int col_stride, const Eigen::PaddingType& padding, Tensor* output,
              TensorFormat data_format);
};

#ifdef GOOGLE_CUDA
template <typename T>
class LaunchConv2DOp<Eigen::GpuDevice, T> {
 public:
  void launch(OpKernelContext* ctx, bool use_cudnn, bool cudnn_use_autotune,
              const Tensor& input, const Tensor& filter, int row_stride,
              int col_stride, const Eigen::PaddingType& padding, Tensor* output,
              TensorFormat data_format);

 private:
  AutoTuneMap<ConvParameters, perftools::gputools::dnn::AlgorithmConfig>
      autotune_results_;
};
#endif  // GOOGLE_CUDA

}  // namespace tensorflow

#endif  // TENSORFLOW_KERNELS_CONV_OPS_H

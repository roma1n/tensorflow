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

// This file contains a set of different implementations of the two-dimensional
// convolution operation. The standard TensorFlow Conv2d kernel uses EigenTensor
// to implement the computation, but this module has a variety of different ways
// of producing the same result. These methods are designed to be easier to
// understand and connect to other libraries, so that we can take advantage of
// platforms that have specialized implementations of GEMM for example.
//
// The basic interface is a Conv functor object that's templated by the types
// of the data it will be operating on, and is passed in the arguments needed to
// calculate the convolution. The simplest implementation of this functor is
// ReferenceConvFunctor, which is a readable but slow reference version.
//
// A faster version uses the approach of packing image patches into a matrix
// before calling a matrix multiply, the Im2ColConvFunctor. In turn, this can
// use a variety of different methods to calculate the matrix multiplication,
// or GEMM. The simplest but slowest is the ReferenceGemmFunctor, but the
// FastGemmFunctor will use whatever optimized libraries are available. By
// default it uses Eigen, but on Apple platforms it will take advantage of the
// system's Accelerate BLAS library to get better performance than the standard
// TensorFlow convolution kernel.
//
// The version actually used is defined at the bottom of this file using the
// REGISTER_KERNEL_BUILDER() macro. To try out different implementations (for
// example to switch to a reference one for easier debugging) you can swap out
// the default functors in that call.
//
// The registration itself is guarded with the USE_GEMM_FOR_CONV macro. The iOS
// makefile build defines this, but if you want to enable this implementation
// and disable the standard EigenTensor one in other build setups, you'll need
// to define it there too.

#include <string.h>
#include <map>
#include <vector>
#include "tensorflow/core/framework/common_shape_fns.h"
#include "tensorflow/core/framework/numeric_op.h"
#include "tensorflow/core/framework/op_kernel.h"
#include "tensorflow/core/framework/register_types.h"
#include "tensorflow/core/framework/resource_mgr.h"
#include "tensorflow/core/framework/tensor.h"
#include "tensorflow/core/framework/tensor_shape.h"
#include "tensorflow/core/framework/tensor_slice.h"
#include "tensorflow/core/kernels/bounds_check.h"
#include "tensorflow/core/util/padding.h"
#include "tensorflow/core/util/tensor_format.h"

#if defined(__APPLE__)
#include <Accelerate/Accelerate.h>
#define USE_ACCELERATE_GEMM
#endif  // __APPLE__

namespace tensorflow {

namespace {
// This function implements the convolution operation in as simple a form as
// possible. It won't give great performance, but it is very useful for
// stepping through and instrumenting for debugging, creating minimal benchmarks
// to prototype with, and sharing with teams that want to run this outside of
// our environment.
// With that in mind, I've avoided using anything except pretty standard C++
// types. This is especially noticeable in the data access through raw array
// indexing. It's deliberate in this case though, since it makes the underlying
// memory order very explicit, which is important for both inspecting memory
// contents during debugging and for specifying what we expect to others.
// The memory layout of the data is, from biggest stride to smallest:
// input_data = [input_batches, input_height, input_width, input_depth]
// filter_data = [filter_height, filter_width, input_depth, filter_count]
// output_data = [input_batches, output_height, output_width, filter_count]
template <class T1, class T2, class T3>
class ReferenceConvFunctor {
 public:
  void operator()(OpKernelContext* context, const T1* input_data,
                  int input_batches, int input_height, int input_width,
                  int input_depth, const T2* filter_data, int filter_height,
                  int filter_width, int filter_count, int stride_rows,
                  int stride_cols, Padding padding, T3* output_data,
                  int output_height, int output_width) {
    // The two different padding modes we support can be a bit confusing. SAME
    // means we're trying to produce an output image that's the same size as the
    // input. It's complicated by stride, which shrinks the output image by a
    // a factor, but it means we end up sampling from outside the borders of the
    // input. These out-of-bounds values are read as zeroes. VALID means only
    // produce output values where the filters can read all their values from
    // within the input image. It effectively removes the margins of the output
    // image compared to the one produced by SAME. Stride complicates this
    // definition though, because it can result in the right and bottom filter
    // patches sampling from outside the borders if it's greater than 1.
    // Most of the logic for sorting this all out is done before this function,
    // when we calculate the output size, but the positioning of the origin of
    // the filters is different between the two modes, since SAME positions the
    // first filter off the edge of the input.
    int filter_left_offset;
    int filter_top_offset;
    if (padding == VALID) {
      filter_left_offset =
          ((output_width - 1) * stride_cols + filter_width - input_width + 1) /
          2;
      filter_top_offset = ((output_height - 1) * stride_rows + filter_height -
                           input_height + 1) /
                          2;
    } else {
      filter_left_offset =
          ((output_width - 1) * stride_cols + filter_width - input_width) / 2;
      filter_top_offset =
          ((output_height - 1) * stride_rows + filter_height - input_height) /
          2;
    }

    // If we've got multiple images in our input, work through each of them.
    for (int batch = 0; batch < input_batches; ++batch) {
      // Walk through all the output image values, sliding the filter to
      // different positions in the input.
      for (int out_y = 0; out_y < output_height; ++out_y) {
        for (int out_x = 0; out_x < output_width; ++out_x) {
          // Each filter kernel produces one output channel.
          for (int out_channel = 0; out_channel < filter_count; ++out_channel) {
            // We're going to calculate a single output value, which means we
            // need to multiply a three dimensional kernel of weights against
            // the current location within the input image.
            /*
             *-------------------------------...
             |\ ^
             | \in_depth
             |  \ v
             |   *-------------------------------...
             |   |            ^
             |   |       in_y_origin
             |   |            v   \
             |   |<in_x_origin>*---*^
             |   |            \|   |filter_height
             .   |             *---*v
             .   |             <--->
             .         filter_width
             .
            */
            const int in_x_origin = (out_x * stride_cols) - filter_left_offset;
            const int in_y_origin = (out_y * stride_rows) - filter_top_offset;
            T3 total(0);
            for (int filter_y = 0; filter_y < filter_height; ++filter_y) {
              for (int filter_x = 0; filter_x < filter_width; ++filter_x) {
                for (int in_channel = 0; in_channel < input_depth;
                     ++in_channel) {
                  const int in_x = in_x_origin + filter_x;
                  const int in_y = in_y_origin + filter_y;
                  T1 input_value;
                  // If the location is outside the bounds of the input image,
                  // use zero as a default value.
                  if ((in_x >= 0) && (in_x < input_width) && (in_y >= 0) &&
                      (in_y < input_height)) {
                    input_value =
                        input_data[(batch * input_height * input_width *
                                    input_depth) +
                                   (in_y * input_width * input_depth) +
                                   (in_x * input_depth) + in_channel];
                  } else {
                    input_value = T1(0);
                  }
                  const T2 filter_value =
                      filter_data[(filter_y * filter_width * input_depth *
                                   filter_count) +
                                  (filter_x * input_depth * filter_count) +
                                  (in_channel * filter_count) + out_channel];
                  total += (input_value * filter_value);
                }
              }
            }
            output_data[(batch * output_height * output_width * filter_count) +
                        (out_y * output_width * filter_count) +
                        (out_x * filter_count) + out_channel] = total;
          }
        }
      }
    }
  }
};

// A readable but slow implementation of matrix multiplication, useful for
// debugging and understanding the algorithm. Use instead of FastGemmFunctor in
// the Im2ColConvFunctor template definition inside the op registration to
// enable. Assumes row-major ordering of the values in memory.
template <class T1, class T2, class T3>
class ReferenceGemmFunctor {
 public:
  void operator()(size_t m, size_t n, size_t k, const T1* a, size_t lda,
                  const T2* b, size_t ldb, T3* c, size_t ldc) {
    const size_t a_i_stride = lda;
    const size_t a_l_stride = 1;
    const size_t b_j_stride = 1;
    const size_t b_l_stride = ldb;
    const size_t c_i_stride = ldc;
    const size_t c_j_stride = 1;
    size_t i, j, l;
    for (j = 0; j < n; j++) {
      for (i = 0; i < m; i++) {
        T3 total(0);
        for (l = 0; l < k; l++) {
          const size_t a_index = ((i * a_i_stride) + (l * a_l_stride));
          const T1 a_value = a[a_index];
          const size_t b_index = ((j * b_j_stride) + (l * b_l_stride));
          const T2 b_value = b[b_index];
          total += (a_value * b_value);
        }
        const size_t c_index = ((i * c_i_stride) + (j * c_j_stride));
        c[c_index] = total;
      }
    }
  }
};

// Uses the optimized Eigen library to implement the matrix multiplication
// required by the Im2ColConvFunctor class. We supply the two input and one
// output types so that the accumulator can potentially be higher-precision than
// the inputs, even though we don't currently take advantage of this.
template <class T1, class T2, class T3>
class FastGemmFunctor {
 public:
  // Convenience wrappers for the Eigen matrix types we'll be using.
  typedef Eigen::Map<
      const Eigen::Matrix<T1, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>
      ConstMatrixT1;
  typedef Eigen::Map<
      const Eigen::Matrix<T2, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>
      ConstMatrixT2;
  typedef Eigen::Map<
      Eigen::Matrix<T3, Eigen::Dynamic, Eigen::Dynamic, Eigen::RowMajor>>
      MatrixT3;
  void operator()(size_t m, size_t n, size_t k, const T1* a, size_t lda,
                  const T2* b, size_t ldb, T3* c, size_t ldc) {
    ConstMatrixT1 a_matrix(a, m, k);
    ConstMatrixT2 b_matrix(b, k, n);
    MatrixT3 c_matrix(c, m, n);
    c_matrix.noalias() = a_matrix * b_matrix;
  }
};

// If we have Apple's Accelerate framework, use their implementation of GEMM to
// get a performance boost for float.
#if defined(USE_ACCELERATE_GEMM)
template <>
class FastGemmFunctor<float, float, float> {
 public:
  void operator()(size_t m, size_t n, size_t k, const float* a, size_t lda,
                  const float* b, size_t ldb, float* c, size_t ldc) {
    cblas_sgemm(CblasRowMajor, CblasNoTrans, CblasNoTrans, m, n, k, 1.0f, a,
                lda, b, ldb, 0.0f, c, ldc);
  }
};
#endif  // USE_ACCELERATE_GEMM

// Used to keep track of persistent memory buffers used within the op.
template <class T, size_t size>
struct Im2ColBufferResource : public ResourceBase {
  mutex mu;
  T data[size];
  string DebugString() { return "Im2ColBufferResource"; }
};

// Implements convolution as a two stage process, first packing the patches of
// the input image into columns (im2col) and then running GEMM to produce the
// final result.
template <class T1, class T2, class T3, class TGemmFunctor>
class Im2ColConvFunctor {
 public:
  void operator()(OpKernelContext* context, const T1* input_data,
                  int input_batches, int input_height, int input_width,
                  int input_depth, const T2* filter_data, int filter_height,
                  int filter_width, int filter_count, int stride_rows,
                  int stride_cols, Padding padding, T3* output_data,
                  int output_height, int output_width) {
    if ((input_batches <= 0) || (input_width <= 0) || (input_height <= 0) ||
        (input_depth <= 0)) {
      LOG(WARNING) << "Conv2D was called with bad input dimensions: "
                   << input_batches << ", " << input_height << ", "
                   << input_width << ", " << input_depth;
      return;
    }
    if ((filter_width <= 0) || (filter_height <= 0) || (filter_count <= 0)) {
      LOG(WARNING) << "Conv2D was called with bad filter dimensions: "
                   << filter_width << ", " << filter_height << ", "
                   << filter_count;
      return;
    }
    if ((output_width <= 0) || (output_height <= 0)) {
      LOG(WARNING) << "Conv2D was called with bad output width or height: "
                   << output_width << ", " << output_height;
      return;
    }

    // These calculations define how the patches will be positioned within the
    // input image. The actual definitions are quite complex, and rely on the
    // previously-calculated output size.
    int filter_left_offset;
    int filter_top_offset;
    if (padding == VALID) {
      filter_left_offset =
          ((output_width - 1) * stride_cols + filter_width - input_width + 1) /
          2;
      filter_top_offset = ((output_height - 1) * stride_rows + filter_height -
                           input_height + 1) /
                          2;
    } else {
      filter_left_offset =
          ((output_width - 1) * stride_cols + filter_width - input_width) / 2;
      filter_top_offset =
          ((output_height - 1) * stride_rows + filter_height - input_height) /
          2;
    }

    // The im2col buffer has # of patches rows, and # of filters cols.
    // It's laid out like this, in row major order in memory:
    //        < filter value count >
    //   ^   +---------------------+
    // patch |                     |
    // count |                     |
    //   v   +---------------------+
    // Each patch row contains a filter_width x filter_height patch of the
    // input, with the depth channel as the most contiguous in memory, followed
    // by the width, then the height. This is the standard memory order in the
    // image world if it helps to visualize it.
    const int filter_value_count = filter_width * filter_height * input_depth;
    const int patch_count = input_batches * output_width * output_height;

    // We don't want to allocate a buffer to hold all the patches if the size is
    // going to be extremely large, so break it into chunks if it's bigger than
    // a limit. Each chunk will be processed serially, so we can refill the
    // buffer for the next chunk and reuse it, keeping maximum memory size down.
    // In this case, we've picked 16 megabytes as a reasonable limit.
    const size_t max_chunk_size = (16 * 1024 * 1024);
    OP_REQUIRES(context, (filter_value_count * sizeof(T1)) <= max_chunk_size,
                errors::InvalidArgument("Im2Col patch too large for buffer"));
    const size_t patches_per_chunk =
        max_chunk_size / (filter_value_count * sizeof(T1));
    const size_t im2col_size = patches_per_chunk * filter_value_count;
    // Because memory allocation is very expensive on mobile platforms, try to
    // allocate a persistent buffer that will be kept around between calls. We
    // use TensorFlow's resource management to ensure that the memory will be
    // released when the session is over.
    Im2ColBufferResource<T1, max_chunk_size>* im2col_buffer_resource;
    std::function<Status(Im2ColBufferResource<T1, max_chunk_size>**)> creator =
        [](Im2ColBufferResource<T1, max_chunk_size>** resource) {
          *resource = new Im2ColBufferResource<T1, max_chunk_size>();
          return Status::OK();
        };
    OP_REQUIRES_OK(context, context->resource_manager()->LookupOrCreate(
                                "Conv2d", "im2col_buffer",
                                &im2col_buffer_resource, creator));
    // This means that multiple ops can't be run simultaneously on different
    // threads, because we have a single shared resource. The platforms this is
    // aimed at have intra-op parallelism as their focus though, so it shouldn't
    // be an issue.
    mutex_lock lock_buffer(im2col_buffer_resource->mu);
    core::ScopedUnref unref_buffer(im2col_buffer_resource);
    T1* im2col_buffer = im2col_buffer_resource->data;

    for (int batch = 0; batch < input_batches; ++batch) {
      const T1* input_batch_start =
          input_data + (batch * input_height * input_width * input_depth);
      for (int out_y = 0; out_y < output_height; ++out_y) {
        const int in_y_origin = (out_y * stride_rows) - filter_top_offset;
        for (int out_x = 0; out_x < output_width; ++out_x) {
          const int in_x_origin = (out_x * stride_cols) - filter_left_offset;
          const int patch_index = (batch * output_width * output_height) +
                                  (out_y * output_width) + out_x;
          const int patch_index_within_chunk = patch_index % patches_per_chunk;
          T1* im2col_patch_start =
              im2col_buffer + (patch_index_within_chunk * filter_value_count);
          for (int filter_y = 0; filter_y < filter_height; ++filter_y) {
            const int in_y = in_y_origin + filter_y;
            T1* im2col_row_start =
                im2col_patch_start + (filter_y * filter_width * input_depth);
            // If we're off the top or the bottom of the input, fill the whole
            // row with zeroes.
            if ((in_y < 0) || (in_y >= input_height)) {
              T1* im2col_row_end =
                  im2col_row_start + (filter_width * input_depth);
              std::fill(im2col_row_start, im2col_row_end, T1(0));
            } else {
              // What we're doing here is trying to copy and fill the im2col
              // buffer as efficiently as possible, using functions to set or
              // duplicate values en masse. We know we don't have to worry about
              // vertical edges because we dealt with that case above, so we
              // just need to handle filters that overlap the left or right
              // edges. Here's what that looks like:
              //
              // < left_zero_count > < center_copy_count > < right_zero_count >
              // +------------------+---------------------+--------------------+
              // |     (filter)     |       (image)       |      (filter)      |
              // +------------------+---------------------+--------------------+
              // in_x_origin        0                 input_width       in_x_end
              //
              // In reality it's unlikely that a filter patch will be wider
              // than an input, but this shows all the edge cases.
              // We use std::fill() to set the left and right sections to zeroes
              // and std::copy() to copy over the input data for the center.
              const int in_x_end = in_x_origin + filter_width;
              const int left_zero_count = std::max(0, 0 - in_x_origin);
              const int right_zero_count = std::max(0, in_x_end - input_width);
              const int center_copy_count =
                  filter_width - (left_zero_count + right_zero_count);
              if (left_zero_count > 0) {
                T1* im2col_left_start = im2col_row_start;
                T1* im2col_left_end =
                    im2col_left_start + (left_zero_count * input_depth);
                std::fill(im2col_left_start, im2col_left_end, T1(0));
              }
              if (center_copy_count > 0) {
                const T1* input_row_start =
                    input_batch_start + (in_y * input_width * input_depth) +
                    (std::max(0, in_x_origin) * input_depth);
                const T1* input_row_end =
                    input_row_start + (center_copy_count * input_depth);
                T1* im2col_center_start =
                    im2col_row_start + (left_zero_count * input_depth);
                std::copy(input_row_start, input_row_end, im2col_center_start);
              }
              if (right_zero_count > 0) {
                T1* im2col_right_start =
                    im2col_row_start +
                    ((left_zero_count + center_copy_count) * input_depth);
                T1* im2col_right_end =
                    im2col_right_start + (right_zero_count * input_depth);
                std::fill(im2col_right_start, im2col_right_end, T1(0));
              }
            }
          }
          const bool is_last_in_chunk =
              (patch_index_within_chunk == (patches_per_chunk - 1));
          const bool is_last_overall =
              ((batch == (input_batches - 1)) &&
               (out_y == (output_height - 1)) && (out_x == (output_width - 1)));
          if (is_last_in_chunk || is_last_overall) {
            // Now we've assembled a set of image patches into a matrix, apply a
            // GEMM matrix multiply of the patches as rows, times the filter
            // weights in columns, to get partial results in the output matrix.
            const int how_many_patches = patch_index_within_chunk + 1;
            const int m = how_many_patches;
            const int n = filter_count;
            const int k = filter_value_count;
            const int lda = filter_value_count;
            const int ldb = filter_count;
            const int ldc = filter_count;
            const size_t start_patch_index =
                patch_index - (how_many_patches - 1);
            T3* chunk_output_data =
                output_data + (start_patch_index * filter_count);
            TGemmFunctor gemm_functor;
            gemm_functor(m, n, k, im2col_buffer, lda, filter_data, ldb,
                         chunk_output_data, ldc);
          }
        }
      }
    }
  }
};

}  // namespace

// This TensorFlow kernel class handles all of the IO and housekeeping for the
// functors that actually implement the underlying algorithm. To swap in
// different implementations of the main calculations, use a different
// TConvFunctor parameter when instantiating the template.
template <class T, class TConvFunctor>
class Conv2DUsingGemmOp : public BinaryOp<T> {
 public:
  explicit Conv2DUsingGemmOp(OpKernelConstruction* context)
      : BinaryOp<T>(context) {
    OP_REQUIRES_OK(context, context->GetAttr("strides", &strides_));
    string data_format;
    OP_REQUIRES_OK(context, context->GetAttr("data_format", &data_format));
    OP_REQUIRES(context, FormatFromString(data_format, &data_format_),
                errors::InvalidArgument("Invalid data format"));
    OP_REQUIRES(context, data_format_ == FORMAT_NHWC,
                errors::InvalidArgument(
                    "Data format not supported by this kernel", data_format));
    OP_REQUIRES(context, strides_.size() == 4,
                errors::InvalidArgument("Sliding window strides field must "
                                        "specify 4 dimensions"));
    const int64 stride_n = GetTensorDim(strides_, data_format_, 'N');
    const int64 stride_c = GetTensorDim(strides_, data_format_, 'C');
    OP_REQUIRES(
        context, stride_n == 1 && stride_c == 1,
        errors::InvalidArgument("Current implementation does not yet support "
                                "strides in the batch and depth dimensions."));
    OP_REQUIRES_OK(context, context->GetAttr("padding", &padding_));
  }

  void Compute(OpKernelContext* context) override {
    // Input tensor is of the following dimensions:
    // [ batch, in_rows, in_cols, in_depth ]
    const Tensor& input = context->input(0);

    // Input filter is of the following dimensions:
    // [ filter_rows, filter_cols, in_depth, out_depth]
    const Tensor& filter = context->input(1);

    // For 2D convolution, there should be 4 dimensions.
    OP_REQUIRES(context, input.dims() == 4,
                errors::InvalidArgument("input must be 4-dimensional",
                                        input.shape().DebugString()));
    OP_REQUIRES(context, filter.dims() == 4,
                errors::InvalidArgument("filter must be 4-dimensional: ",
                                        filter.shape().DebugString()));

    for (int i = 0; i < 3; i++) {
      OP_REQUIRES(context, FastBoundsCheck(filter.dim_size(i),
                                           std::numeric_limits<int>::max()),
                  errors::InvalidArgument("filter too large"));
    }

    // The last dimension for input is in_depth. It must be the same as the
    // filter's in_depth.
    const int64 in_depth = GetTensorDim(input, data_format_, 'C');
    OP_REQUIRES(
        context, in_depth == filter.dim_size(2),
        errors::InvalidArgument("input and filter must have the same depth: ",
                                in_depth, " vs ", filter.dim_size(2)));

    // The last dimension for filter is out_depth.
    const int out_depth = static_cast<int>(filter.dim_size(3));

    // The second dimension for input is rows/height.
    // The first dimension for filter is rows/height.
    const int64 input_rows_raw = GetTensorDim(input, data_format_, 'H');
    OP_REQUIRES(context, FastBoundsCheck(input_rows_raw,
                                         std::numeric_limits<int>::max()),
                errors::InvalidArgument("Input rows too large"));
    const int input_rows = static_cast<int>(input_rows_raw);
    const int filter_rows = static_cast<int>(filter.dim_size(0));

    // The third dimension for input is columns/width.
    // The second dimension for filter is columns/width.
    const int64 input_cols_raw = GetTensorDim(input, data_format_, 'W');
    OP_REQUIRES(context, FastBoundsCheck(input_cols_raw,
                                         std::numeric_limits<int>::max()),
                errors::InvalidArgument("Input cols too large"));
    const int input_cols = static_cast<int>(input_cols_raw);
    const int filter_cols = static_cast<int>(filter.dim_size(1));

    // The first dimension for input is batch.
    const int64 batch_raw = GetTensorDim(input, data_format_, 'N');
    OP_REQUIRES(context,
                FastBoundsCheck(batch_raw, std::numeric_limits<int>::max()),
                errors::InvalidArgument("batch is too large"));
    const int batch = static_cast<int>(batch_raw);

    // For now we take the stride from the second and third dimensions only (we
    // do not support striding on the batch or depth dimension).
    const int stride_rows = GetTensorDim(strides_, data_format_, 'H');
    const int stride_cols = GetTensorDim(strides_, data_format_, 'W');

    int64 out_rows = 0, out_cols = 0, pad_rows = 0, pad_cols = 0;
    OP_REQUIRES_OK(context,
                   GetWindowedOutputSize(input_rows, filter_rows, stride_rows,
                                         padding_, &out_rows, &pad_rows));
    OP_REQUIRES_OK(context,
                   GetWindowedOutputSize(input_cols, filter_cols, stride_cols,
                                         padding_, &out_cols, &pad_cols));
    TensorShape out_shape =
        ShapeFromFormat(data_format_, batch, out_rows, out_cols, out_depth);

    // Output tensor is of the following dimensions:
    // [ in_batch, out_rows, out_cols, out_depth ]
    Tensor* output = nullptr;
    OP_REQUIRES_OK(context, context->allocate_output(0, out_shape, &output));

    VLOG(2) << "Conv2D: in_depth = " << in_depth
            << ", input_cols = " << input_cols
            << ", filter_cols = " << filter_cols
            << ", input_rows = " << input_rows
            << ", filter_rows = " << filter_rows
            << ", stride_rows = " << stride_rows
            << ", stride_cols = " << stride_cols
            << ", out_depth = " << out_depth;

    // If there is nothing to compute, return.
    if (out_shape.num_elements() == 0) {
      return;
    }
    TConvFunctor conv_functor;
    conv_functor(context, input.flat<T>().data(), batch, input_rows, input_cols,
                 in_depth, filter.flat<T>().data(), filter_rows, filter_cols,
                 out_depth, stride_rows, stride_cols, padding_,
                 output->flat<T>().data(), out_rows, out_cols);
  }

 private:
  std::vector<int32> strides_;
  Padding padding_;
  TensorFormat data_format_;

  TF_DISALLOW_COPY_AND_ASSIGN(Conv2DUsingGemmOp);
};

#define REGISTER_CPU(T)                                         \
  REGISTER_KERNEL_BUILDER(                                      \
      Name("Conv2D").Device(DEVICE_CPU).TypeConstraint<T>("T"), \
      Conv2DUsingGemmOp<                                        \
          T, Im2ColConvFunctor<T, T, T, FastGemmFunctor<T, T, T>>>);

// Only register this GEMM-based implementation of Conv2d if the compiler flags
// request the implementation explicitly, since otherwise it will clash with the
// default EigenTensor-based kernel.
#if defined(USE_GEMM_FOR_CONV)
TF_CALL_half(REGISTER_CPU);
TF_CALL_float(REGISTER_CPU);
#endif  // USE_GEMM_FOR_CONV

}  // namespace tensorflow

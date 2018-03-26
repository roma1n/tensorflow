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

#ifndef TENSORFLOW_C_C_API_EXPERIMENTAL_H_
#define TENSORFLOW_C_C_API_EXPERIMENTAL_H_

#include <stddef.h>
#include <stdint.h>

#include "tensorflow/c/c_api.h"

// --------------------------------------------------------------------------
// Experimental C API for TensorFlow.
//
// The API here is subject to changes in the future.
// --------------------------------------------------------------------------

// Macro to control visibility of exported symbols in the shared library (.so,
// .dylib, .dll).
// This duplicates the TF_EXPORT macro definition in
// tensorflow/core/platform/macros.h in order to keep this .h file independent
// of any other includes.$a
#ifdef SWIG
#define TF_CAPI_EXPORT
#else
#if defined(COMPILER_MSVC)
#ifdef TF_COMPILE_LIBRARY
#define TF_CAPI_EXPORT __declspec(dllexport)
#else
#define TF_CAPI_EXPORT __declspec(dllimport)
#endif  // TF_COMPILE_LIBRARY
#else
#define TF_CAPI_EXPORT __attribute__((visibility("default")))
#endif  // COMPILER_MSVC
#endif  // SWIG

#ifdef __cplusplus
extern "C" {
#endif

// When `enable` is true, set
// tensorflow.ConfigProto.OptimizerOptions.global_jit_level to ON_1, and also
// set XLA flag values to prepare for XLA compilation. Otherwise set
// global_jit_level to OFF.
//
// This API is syntax sugar over TF_SetConfig(), and is used by clients that
// cannot read/write the tensorflow.ConfigProto proto.
TF_CAPI_EXPORT extern void TF_EnableXLACompilation(TF_SessionOptions* options,
                                                   unsigned char enable);

// Initializes TPU system. Must be called exactly once before TF_SessionRun() is
// called on a TPU graph.
//
// The session graph must contain a node named ConfigureDistributedTPU.
// TODO(b/74774824): Improve the API on initializing TPU system.
TF_CAPI_EXPORT extern void TF_InitializeTPU(TF_Session* session,
                                            TF_Status* status);

// Shuts down TPU system. For any `session` where TF_InitializeTPU() has
// been successfully called, this call must be made exactly once before the
// session is closed.
// The session graph must contain a node named ShutdownDistributedTPU.
TF_CAPI_EXPORT extern void TF_ShutdownTPU(TF_Session* session,
                                          TF_Status* status);

// Returns the graph content in a human-readable format, with length set in
// `len`. The format is subject to change in the future.
// The returned string is heap-allocated, and caller should call free() on it.
TF_CAPI_EXPORT extern const char* TF_GraphDebugString(TF_Graph* graph,
                                                      size_t* len);

// Returns the graph content in a human-readable format, with length set in
// `len`. The format is subject to change in the future.
// The returned string is heap-allocated, and caller should call free() on it.
TF_CAPI_EXPORT extern const char* TF_GraphDebugString(TF_Graph* graph,
                                                      size_t* len);

// Creates a stack of data set + iterator nodes, currently hard-coded to return
// a sequence of 3 float values <42.0, 43.0, 44.0> over 3 calls. On success,
// returns the IteratorGetNext node, which caller can run or feed into an node.
//
// TODO(hongm): Extend the API to allow customization of the nodes created.
TF_CAPI_EXPORT extern TF_Operation* TF_MakeFakeIteratorGetNextWithDatasets(
    TF_Graph* graph, TF_Status* status);

// Similar to the above API, except that the returned iterator reads the
// TFRecord files from `file_path`.
// The iterators outputs 2 tensors:
// - A float tensor of shape `batch_size` X 224 X 224 X 3
// - An int32 tensor of shape `batch_size`
// TODO(hongm): Extend the API to allow customization of the nodes created.
TF_CAPI_EXPORT extern TF_Operation* TF_MakeImagenetIteratorGetNextWithDatasets(
    TF_Graph* graph, const char* file_path, int batch_size, TF_Status* status);

#ifdef __cplusplus
} /* end extern "C" */
#endif

#endif  // TENSORFLOW_C_C_API_EXPERIMENTAL_H_

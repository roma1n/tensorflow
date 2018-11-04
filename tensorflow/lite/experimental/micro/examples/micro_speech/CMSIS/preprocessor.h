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

#ifndef TENSORFLOW_CONTRIB_LITE_EXPERIMENTAL_MICRO_EXAMPLES_MICRO_SPEECH_PREPROCESSOR_H_
#define TENSORFLOW_CONTRIB_LITE_EXPERIMENTAL_MICRO_EXAMPLES_MICRO_SPEECH_PREPROCESSOR_H_

#include "tensorflow/lite/c/c_api_internal.h"
#include "tensorflow/lite/experimental/micro/micro_error_reporter.h"

TfLiteStatus Preprocess(tflite::ErrorReporter* error_reporter,
                        const int16_t* input, int input_size, int output_size,
                        uint8_t* output);

#endif  // TENSORFLOW_CONTRIB_LITE_EXPERIMENTAL_MICRO_EXAMPLES_MICRO_SPEECH_PREPROCESSOR_H_

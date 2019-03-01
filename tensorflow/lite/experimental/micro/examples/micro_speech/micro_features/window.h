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
#ifndef TENSORFLOW_LITE_EXPERIMENTAL_MICRO_EXAMPLES_MICRO_SPEECH_MICRO_FEATURES_WINDOW_H_
#define TENSORFLOW_LITE_EXPERIMENTAL_MICRO_EXAMPLES_MICRO_SPEECH_MICRO_FEATURES_WINDOW_H_

#include <stdint.h>
#include <stdlib.h>

#include "tensorflow/lite/experimental/micro/examples/micro_speech/micro_features/micro_model_settings.h"

#define kFrontendWindowBits 12

struct WindowState {
  size_t size;
  int16_t coefficients[kMaxAudioSampleSize];
  size_t step;

  int16_t input[kMaxAudioSampleSize];
  size_t input_used;
  int16_t output[kMaxAudioSampleSize];
  int16_t max_abs_output_value;
};

// Applies a window to the samples coming in, stepping forward at the given
// rate.
int WindowProcessSamples(struct WindowState* state, const int16_t* samples,
                         size_t num_samples, size_t* num_samples_read);

void WindowReset(struct WindowState* state);

#endif  // TENSORFLOW_LITE_EXPERIMENTAL_MICRO_EXAMPLES_MICRO_SPEECH_MICRO_FEATURES_WINDOW_H_

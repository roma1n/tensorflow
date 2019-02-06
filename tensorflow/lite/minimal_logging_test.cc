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

#include "tensorflow/lite/minimal_logging.h"

#include <string>

#include <gtest/gtest.h>

namespace tflite {

TEST(MinimalLogging, Basic) {
  testing::internal::CaptureStderr();
  TFLITE_LOG_PROD(TFLITE_LOG_INFO, "Foo");
  EXPECT_EQ("INFO: Foo\n", testing::internal::GetCapturedStderr());
}

TEST(MinimalLogging, BasicFormatted) {
  testing::internal::CaptureStderr();
  TFLITE_LOG_PROD(TFLITE_LOG_INFO, "Foo %s %s", "Bar", "Baz");
  EXPECT_EQ("INFO: Foo Bar Baz\n", testing::internal::GetCapturedStderr());
}

TEST(MinimalLogging, Warn) {
  testing::internal::CaptureStderr();
  TFLITE_LOG_PROD(TFLITE_LOG_WARNING, "One", "");
  EXPECT_EQ("WARNING: One\n", testing::internal::GetCapturedStderr());
}

TEST(MinimalLogging, Error) {
  testing::internal::CaptureStderr();
  TFLITE_LOG_PROD(TFLITE_LOG_ERROR, "Two");
  EXPECT_EQ("ERROR: Two\n", testing::internal::GetCapturedStderr());
}

TEST(MinimalLogging, UnknownSeverity) {
  testing::internal::CaptureStderr();
  TFLITE_LOG_PROD(static_cast<LogSeverity>(-1), "Three");
  EXPECT_EQ("<Unknown severity>: Three\n",
            testing::internal::GetCapturedStderr());
}

}  // namespace tflite

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}

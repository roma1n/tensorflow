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

#include <fcntl.h>
#ifndef _WIN32
#include <sys/mman.h>
#else
#include <windows.h>
#endif
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "tensorflow/lite/allocation.h"
#include "tensorflow/lite/core/api/error_reporter.h"

namespace tflite {

#ifdef _WIN32
static constexpr void* MAP_FAILED = nullptr;

MMAPAllocation::MMAPAllocation(const char* filename,
                               ErrorReporter* error_reporter)
    : Allocation(error_reporter)
    , mmapped_buffer_(MAP_FAILED)
    , file_handle_(nullptr)
    , file_mapping_(nullptr) {

  file_handle_ = CreateFile(filename, GENERIC_READ, 0, NULL, OPEN_EXISTING, 0, 0);
  if (file_handle_ == INVALID_HANDLE_VALUE) {
    error_reporter_->Report("Could not open '%s'.", filename);
    return;
  }

  buffer_size_bytes_ = GetFileSize( file_handle_, nullptr );

  file_mapping_ = CreateFileMapping(file_handle_, NULL, PAGE_READONLY, 0, 0, NULL);
  if (file_mapping_ == nullptr)
    return;

  mmapped_buffer_ = MapViewOfFile(file_mapping_, FILE_MAP_READ, 0, 0, buffer_size_bytes_);

  if (mmapped_buffer_ == MAP_FAILED) {
    error_reporter_->Report("Mmap of '%s' failed.", filename);
    return;
  }
}

MMAPAllocation::~MMAPAllocation() {
  if (valid()) {
     UnmapViewOfFile( mmapped_buffer_ );
  }

  if (file_mapping_ != nullptr) {
    CloseHandle( file_mapping_ );
  }

  if (file_handle_ != nullptr){
    CloseHandle(file_handle_);
  }
}

#else
MMAPAllocation::MMAPAllocation(const char* filename,
                               ErrorReporter* error_reporter)
    : Allocation(error_reporter), mmapped_buffer_(MAP_FAILED) {
  mmap_fd_ = open(filename, O_RDONLY);
  if (mmap_fd_ == -1) {
    error_reporter_->Report("Could not open '%s'.", filename);
    return;
  }
  struct stat sb;
  fstat(mmap_fd_, &sb);
  buffer_size_bytes_ = sb.st_size;
  mmapped_buffer_ =
      mmap(nullptr, buffer_size_bytes_, PROT_READ, MAP_SHARED, mmap_fd_, 0);
  if (mmapped_buffer_ == MAP_FAILED) {
    error_reporter_->Report("Mmap of '%s' failed.", filename);
    return;
  }
}

MMAPAllocation::~MMAPAllocation() {
  if (valid()) {
    munmap(const_cast<void*>(mmapped_buffer_), buffer_size_bytes_);
  }
  if (mmap_fd_ != -1) close(mmap_fd_);
}
#endif

const void* MMAPAllocation::base() const { return mmapped_buffer_; }

size_t MMAPAllocation::bytes() const { return buffer_size_bytes_; }

bool MMAPAllocation::valid() const { return mmapped_buffer_ != MAP_FAILED; }

bool MMAPAllocation::IsSupported() { return true; }

}  // namespace tflite

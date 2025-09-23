///
/// @file ExecutableMemory.cpp
/// @copyright Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
/// SPDX-License-Identifier: Apache-2.0
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
#include <cerrno>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <new>
#include <ostream>
#include <utility>

#include "ExecutableMemory.hpp"
#include "MemUtils.hpp"

#include "src/config.hpp"
#ifdef __linux__
#include <unistd.h>
#endif

namespace vb {

ExecutableMemory::ExecutableMemory(uint8_t const *const data, size_t const size) : ExecutableMemory(nullptr, size, -1) {
  init(data);
}
ExecutableMemory::ExecutableMemory(ExecutableMemory &&other) VB_NOEXCEPT : data_(other.data_), size_(other.size_), fd_(other.fd_) {
  other.data_ = nullptr;
  other.size_ = 0U;
  other.fd_ = -1;
}
// coverity[autosar_cpp14_a6_2_1_violation]
ExecutableMemory &ExecutableMemory::operator=(ExecutableMemory &&other) & VB_NOEXCEPT {
  swap(*this, std::move(other));
  return *this;
}

ExecutableMemory::~ExecutableMemory() VB_NOEXCEPT {
  if (data_ != nullptr) {
    this->freeExecutableMemory();
  }
}

void ExecutableMemory::init(uint8_t const *const data) {
  if (size_ == 0U) {
    return;
  }

  MemUtils::MmapMemory const mmapMemory{MemUtils::allocPagedMemory(size_)};
  uint8_t *const readWriteMemory{mmapMemory.ptr};

  if (readWriteMemory != nullptr) {
    fd_ = mmapMemory.fd;
    MemUtils::memcpyAndClearInstrCache(readWriteMemory, data, size_);
  } else {
    throw std::bad_alloc(); // GCOVR_EXCL_LINE
  }

#ifdef __linux__
  uint8_t *const readExecuteMemory{MemUtils::mapRXMemory(size_, fd_)};
  data_ = readExecuteMemory;
  MemUtils::freePagedMemory(readWriteMemory, size_);
#else
  data_ = readWriteMemory;
  MemUtils::setPermissionRX(data_, size_);
#endif
}

void ExecutableMemory::freeExecutableMemory() const VB_NOEXCEPT {
  if (data_ != nullptr) {
#ifdef VB_WIN32
    MemUtils::setPermissionRW(data_, size_);
#endif
    MemUtils::freePagedMemory(data_, size_);
#ifdef __linux__
    if (fd_ != -1) {
      int32_t const error{close(fd_)};
      if (error != 0) {
        std::cout << "Closing mapped file failed " << errno << &std::endl;
      }
    }
#endif
  }
}

} // namespace vb

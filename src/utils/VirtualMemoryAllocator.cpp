///
/// @file VirtualMemoryAllocator.cpp
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
#include <atomic>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <new>

#include "LinearMemoryAllocator.hpp"
#include "MemUtils.hpp"
#include "VirtualMemoryAllocator.hpp"

#include "src/config.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/util.hpp"
#ifndef NVALGRIND
#include <valgrind/memcheck.h>
#endif

namespace vb {

VirtualMemoryAllocator::VirtualMemoryAllocator() VB_NOEXCEPT : VirtualMemoryAllocator(0U) {
}

// coverity[autosar_cpp14_a12_1_1_violation] Fake positive
// coverity[autosar_cpp14_a12_6_1_violation]
VirtualMemoryAllocator::VirtualMemoryAllocator(size_t const totalSize) VB_NOEXCEPT : committedSize_(0U),
                                                                                     totalSize_(MemUtils::roundUpToOSMemoryPageSize(totalSize)) {
  if (totalSize_ > 0U) {
    // Actually allocate something
    data_ = pCast<uint8_t *>(MemUtils::allocVirtualMemory(totalSize_));

#ifndef NVALGRIND
    static_cast<void>(VALGRIND_DISABLE_ADDR_ERROR_REPORTING_IN_RANGE(data_, totalSize_));
#endif
  } else {
    data_ = nullptr;
  }
}

VirtualMemoryAllocator::VirtualMemoryAllocator(VirtualMemoryAllocator &&other) VB_NOEXCEPT : data_(other.data_),
                                                                                             // coverity[autosar_cpp14_a8_4_5_violation]
                                                                                             // coverity[autosar_cpp14_a12_8_4_violation]
                                                                                             committedSize_(other.committedSize_.load()),
                                                                                             // coverity[autosar_cpp14_a8_4_5_violation]
                                                                                             // coverity[autosar_cpp14_a12_8_4_violation]
                                                                                             totalSize_(other.totalSize_.load()) {
  other.data_ = nullptr;
  // coverity[autosar_cpp14_a8_4_5_violation]
  other.committedSize_ = 0U;
  // coverity[autosar_cpp14_a8_4_5_violation]
  other.totalSize_ = 0U;
}

// coverity[autosar_cpp14_a6_2_1_violation]
VirtualMemoryAllocator &VirtualMemoryAllocator::operator=(VirtualMemoryAllocator &&other) & VB_NOEXCEPT {
  swap(*this, std::move(other));
  return *this;
}

size_t VirtualMemoryAllocator::resize(size_t const size) {
  assert(size % MemUtils::getOSMemoryPageSize() == 0 && "Size must be a multiple of OS page size");

  if (size > totalSize_) {
    throw std::bad_alloc();
  }

  if (size > committedSize_) {
    size_t const extensionSize{size - committedSize_};
    uint8_t *const commitAddress{pAddI(data_, committedSize_.load())};
    MemUtils::commitVirtualMemory(commitAddress, extensionSize);
  } else if (size < committedSize_) {
    size_t const reductionSize{committedSize_ - size};
    uint8_t *const uncommitAddress{pAddI(data_, size)};
    MemUtils::uncommitVirtualMemory(uncommitAddress, reductionSize);
  } else {
    static_cast<void>(0);
  }

  committedSize_ = size;

  return size;
}

size_t VirtualMemoryAllocator::roundUpResize(size_t const recommendSize) {
  size_t const pageAlignedSize{MemUtils::roundUpToOSMemoryPageSize(recommendSize)};
  return resize(pageAlignedSize);
}

void VirtualMemoryAllocator::freeVirtualMemory() const VB_NOEXCEPT {
  if (data_ != nullptr) {
#ifndef NVALGRIND
    static_cast<void>(VALGRIND_ENABLE_ADDR_ERROR_REPORTING_IN_RANGE(data_, totalSize_));
#endif

    try {
      MemUtils::freeVirtualMemory(data_, totalSize_);
    } catch (vb::RuntimeError const &e) {
      static_cast<void>(e);
      std::terminate();
    }
    // GCOVR_EXCL_START
    catch (...) {
      UNREACHABLE(return 0U, "Should catch other exception than vb::RuntimeError");
    }
    // GCOVR_EXCL_STOP
  }
}

VirtualMemoryAllocator::~VirtualMemoryAllocator() VB_NOEXCEPT {
  this->freeVirtualMemory();
}

} // namespace vb

///
/// @file ExtendableMemory.cpp
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
#include <cstdint>
#include <utility>

#include "ExtendableMemory.hpp"

#include "src/config.hpp"
#include "src/core/common/VbExceptions.hpp"

namespace vb {

ExtendableMemory::ExtendableMemory() VB_NOEXCEPT : ExtendableMemory(nullptr, nullptr, 0U) {
}
ExtendableMemory::ExtendableMemory(ReallocFnc const extensionRequestPtr, uint8_t *const data, uint32_t const size, void *const ctx) VB_NOEXCEPT
    : data_(data),
      size_(size),
      extensionRequestPtr_(extensionRequestPtr),
      ctx_(ctx) {
}
ExtendableMemory::ExtendableMemory(ExtendableMemory &&other) VB_NOEXCEPT : data_(other.data_),
                                                                           size_(other.size_),
                                                                           extensionRequestPtr_(other.extensionRequestPtr_),
                                                                           ctx_(other.ctx_) {
  other.extensionRequestPtr_ = nullptr;
}

ExtendableMemory::~ExtendableMemory() VB_NOEXCEPT {
  this->freeExtendableMemory();
}

ExtendableMemory &ExtendableMemory::operator=(ExtendableMemory &&original) & VB_NOEXCEPT {
  swap(*this, std::move(original));
  return *this;
}

void ExtendableMemory::resize(uint32_t const size) {
  if (size_ >= size) {
    return;
  }

  if (extensionRequestPtr_ != nullptr) {
    extensionRequest(size);
    if ((data_ != nullptr) && (size_ >= size)) {
      return;
    }
  }

  throw RuntimeError(ErrorCode::Could_not_extend_memory);
}

// coverity[autosar_cpp14_m9_3_3_violation]
void ExtendableMemory::freeExtendableMemory() VB_NOEXCEPT {
  if (extensionRequestPtr_ != nullptr) {
    extensionRequestPtr_(*this, 0U, ctx_);
  }
}

} // namespace vb

///
/// @file ActiveMemoryManager.cpp
/// @copyright Copyright (C) 2026 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
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

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "ActiveMemoryManager.hpp"

#include "src/config.hpp"
#include "src/core/common/ExtendableMemory.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/WasmConstants.hpp"
#include "src/core/runtime/IMemoryManager.hpp"

namespace vb {

// coverity[autosar_cpp14_a16_2_3_violation]
// coverity[autosar_cpp14_a12_1_1_violation]
ActiveMemoryManager::ActiveMemoryManager(ReallocFnc const extensionRequestPtr, void *const ctx) VB_NOEXCEPT
    : jobMemory_(extensionRequestPtr, nullptr, 0U, ctx),
      basedataStart_(nullptr),
      basedataSize_(0U),
      allowedLinMemPages_(0U),
      usableLinMemBytes_(0U),
      maxDesiredRamOnMemoryExtendFailed_(0U) {
}

void ActiveMemoryManager::init(uint32_t const basedataSize, uint32_t const initialLinMemPages) {
  basedataSize_ = basedataSize;
  allowedLinMemPages_ = initialLinMemPages;
  usableLinMemBytes_ = 0U;
  maxDesiredRamOnMemoryExtendFailed_ = 0U;

  if (!ensureCapacityForLinearSize(0U)) {
    throw RuntimeError(ErrorCode::Could_not_extend_linear_memory);
  }
  syncBasedataStart();
}

uint8_t *ActiveMemoryManager::getBasedataStart() const VB_NOEXCEPT {
  return basedataStart_;
}

bool ActiveMemoryManager::extend(uint32_t const newTotalLinMemPages) VB_NOEXCEPT {
  allowedLinMemPages_ = newTotalLinMemPages;
  return true;
}

bool ActiveMemoryManager::shrink(uint32_t const minimumLength) VB_NOEXCEPT {
  if (!jobMemory_.hasExtensionRequest()) {
    // Legacy active behavior without a callback is a no-op shrink.
    return true;
  }

  if (minimumLength > usableLinMemBytes_) {
    return false;
  }

  uint64_t const requestedTotalSize{static_cast<uint64_t>(basedataSize_) + static_cast<uint64_t>(minimumLength)};
  if (requestedTotalSize > UINT32_MAX) {
    return false;
  }

  uint32_t roundedRequiredSize{static_cast<uint32_t>(requestedTotalSize)};
  if ((roundedRequiredSize & 1U) != 0U) {
    if (roundedRequiredSize == UINT32_MAX) {
      return false;
    }
    roundedRequiredSize++;
  }

  if (roundedRequiredSize < jobMemory_.size()) {
    jobMemory_.extensionRequest(roundedRequiredSize);
    if ((jobMemory_.data() == nullptr) || (jobMemory_.size() < roundedRequiredSize)) {
      return false;
    }
    syncBasedataStart();
  }

  usableLinMemBytes_ = minimumLength;
  return true;
}

IMemoryManager::ProbeResult ActiveMemoryManager::probe(uint32_t const linMemOffset) VB_NOEXCEPT {
  if (linMemOffset < usableLinMemBytes_) {
    return ProbeResult::Ok;
  }

  uint64_t const allowedSizeInBytes{static_cast<uint64_t>(allowedLinMemPages_) * WasmConstants::wasmPageSize};
  if (static_cast<uint64_t>(linMemOffset) >= allowedSizeInBytes) {
    return ProbeResult::OutOfBounds;
  }

  if (!ensureLinearSize(linMemOffset + 1U)) {
    return ProbeResult::AllocationFailure;
  }

  return ProbeResult::Ok;
}

uint32_t ActiveMemoryManager::getLinearMemorySize() const VB_NOEXCEPT {
  return usableLinMemBytes_;
}

uint64_t ActiveMemoryManager::getMaxDesiredRamOnMemoryExtendFailed() const VB_NOEXCEPT {
  return maxDesiredRamOnMemoryExtendFailed_;
}

uint32_t ActiveMemoryManager::getAllocationSize() const VB_NOEXCEPT {
  return jobMemory_.size();
}

uint8_t *const *ActiveMemoryManager::getBasedataStartPtrPtr() const VB_NOEXCEPT {
  return &basedataStart_;
}

bool ActiveMemoryManager::ensureCapacityForLinearSize(uint32_t const requiredLinearBytes) VB_NOEXCEPT {
  uint64_t const requiredTotalSize{static_cast<uint64_t>(basedataSize_) + static_cast<uint64_t>(requiredLinearBytes)};
  if (requiredTotalSize > UINT32_MAX) {
    maxDesiredRamOnMemoryExtendFailed_ = requiredTotalSize;
    return false;
  }

  uint32_t roundedRequiredSize{static_cast<uint32_t>(requiredTotalSize)};
  if ((roundedRequiredSize & 1U) != 0U) {
    if (roundedRequiredSize == UINT32_MAX) {
      maxDesiredRamOnMemoryExtendFailed_ = static_cast<uint64_t>(UINT32_MAX) + 1U;
      return false;
    }
    roundedRequiredSize++;
  }

  if (jobMemory_.size() >= roundedRequiredSize) {
    return true;
  }

  if (!jobMemory_.hasExtensionRequest()) {
    maxDesiredRamOnMemoryExtendFailed_ = static_cast<uint64_t>(roundedRequiredSize);
    return false;
  }

  jobMemory_.extensionRequest(roundedRequiredSize);
  bool const success{(jobMemory_.data() != nullptr) && (jobMemory_.size() >= roundedRequiredSize)};
  if (success) {
    syncBasedataStart();
  } else {
    maxDesiredRamOnMemoryExtendFailed_ = static_cast<uint64_t>(roundedRequiredSize);
  }
  return success;
}

bool ActiveMemoryManager::ensureLinearSize(uint32_t const requiredLinearBytes) VB_NOEXCEPT {
  if (requiredLinearBytes <= usableLinMemBytes_) {
    return true;
  }

  if (!ensureCapacityForLinearSize(requiredLinearBytes)) {
    return false;
  }

  uint8_t *const basedataStart{jobMemory_.data()};
  if (basedataStart == nullptr) {
    return false;
  }

  uint8_t *const linearMemoryStart{basedataStart + basedataSize_};
  static_cast<void>(
      std::memset(linearMemoryStart + usableLinMemBytes_, 0x00, static_cast<size_t>(requiredLinearBytes) - static_cast<size_t>(usableLinMemBytes_)));
  usableLinMemBytes_ = requiredLinearBytes;
  return true;
}

void ActiveMemoryManager::syncBasedataStart() VB_NOEXCEPT {
  basedataStart_ = jobMemory_.data();
}

} // namespace vb

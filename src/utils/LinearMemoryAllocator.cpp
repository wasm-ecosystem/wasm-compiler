///
/// @file LinearMemoryAllocator.cpp
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
#include <utility>

#include "LinearMemoryAllocator.hpp"
#include "MemUtils.hpp"

#include "src/config.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/WasmConstants.hpp"
#include "src/core/common/util.hpp"
#include "src/utils/VirtualMemoryAllocator.hpp"

namespace vb {

LinearMemoryAllocator::LinearMemoryAllocator() VB_NOEXCEPT : pagedBasedataSize_(0U),
                                                             linMemPages_(0U),
                                                             pagedMemoryLimit_(UINT64_MAX),
                                                             maxDesiredRamOnMemoryExtendFailed_(0U) {
}

LinearMemoryAllocator::LinearMemoryAllocator(LinearMemoryAllocator &&other) VB_NOEXCEPT
    : virtualMemoryAllocator_(std::move(other.virtualMemoryAllocator_)),
      pagedBasedataSize_(other.pagedBasedataSize_),
      linMemPages_(other.linMemPages_),
      // coverity[autosar_cpp14_a8_4_5_violation]
      // coverity[autosar_cpp14_a12_8_4_violation]
      pagedMemoryLimit_(other.pagedMemoryLimit_.load()),
      maxDesiredRamOnMemoryExtendFailed_(other.maxDesiredRamOnMemoryExtendFailed_) {
}

// coverity[autosar_cpp14_a6_2_1_violation]
LinearMemoryAllocator &LinearMemoryAllocator::operator=(LinearMemoryAllocator &&other) & VB_NOEXCEPT {
  swap(*this, std::move(other));
  return *this;
}

void *LinearMemoryAllocator::init(uint32_t const basedataSize, uint32_t const initialLinMemPages) {
  static_assert(sizeof(size_t) > sizeof(uint32_t), "platform with unsupported size_t");
  assert(initialLinMemPages <= WasmConstants::maxWasmPages && "Too many pages");

  pagedBasedataSize_ = MemUtils::roundUpToOSMemoryPageSize(static_cast<size_t>(basedataSize));
  linMemPages_ = initialLinMemPages;

  // Explicitly destruct the allocator and construct a new one in its place with maximum total size the job memory can
  // have plus a 4GB guard region after that This is needed because Wasm memory accesses are the sum of a 32-bit address
  // and a 32-bit offset
  virtualMemoryAllocator_ = VirtualMemoryAllocator(pagedBasedataSize_ + WasmConstants::maxLinearMemorySize + offsetGuardRegionSize);

// Commit initial basedata memory
#if !EAGER_ALLOCATION
  size_t const initialCommit{pagedBasedataSize_};
#else
  size_t const initialCommit = pagedBasedataSize_ + (static_cast<size_t>(initialLinMemPages) * WasmConstants::wasmPageSize);
#endif
  // coverity[autosar_cpp14_a15_0_2_violation]
  if (!commit(initialCommit)) {
    virtualMemoryAllocator_.freeVirtualMemory();
    // coverity[autosar_cpp14_a15_0_2_violation]
    throw RuntimeError(ErrorCode::Could_not_extend_linear_memory);
  }

  uint8_t *const virtualMemoryStart{pCast<uint8_t *>(virtualMemoryAllocator_.data())};
  uint8_t *const linearMemoryStart{pAddI(virtualMemoryStart, pagedBasedataSize_)};
  assert(pToNum(linearMemoryStart) % 8 == 0 && "Linear memory must be 8B aligned");

  uint8_t *const jobMemoryStart{pSubI(linearMemoryStart, basedataSize)};
  assert(pToNum(jobMemoryStart) % 8 == 0 && "Module memory must be 8B aligned");

  return jobMemoryStart;
}

bool LinearMemoryAllocator::extend(uint32_t const newTotalLinMemPages) VB_NOEXCEPT {
  linMemPages_ = newTotalLinMemPages;

#if EAGER_ALLOCATION
  if (virtualMemoryAllocator_.data() != nullptr) {
    try {
      size_t const newRuntimeMemorySize{pagedBasedataSize_ + (static_cast<size_t>(newTotalLinMemPages) * WasmConstants::wasmPageSize)};
      return commit(newRuntimeMemorySize);
    } catch (vb::RuntimeError const &e) {
      static_cast<void>(e);
      return false;
    }
    // GCOVR_EXCL_START
    catch (...) {
      UNREACHABLE(return false, "Should catch other exception than vb::RuntimeError");
    }
    // GCOVR_EXCL_STOP
  }
#endif
  return true;
}

bool LinearMemoryAllocator::shrink(uint32_t const minimumLength) VB_NOEXCEPT {
  if (virtualMemoryAllocator_.data() == nullptr) {
    return false;
  }
  size_t const linMemCommitedSize{virtualMemoryAllocator_.getCommitedSize() - pagedBasedataSize_};
  if (linMemCommitedSize < static_cast<size_t>(minimumLength)) {
    // The shrink offset is bigger than commited size, cannot shrink
    return false;
  }
  try {
    size_t const newRuntimeMemorySize{pagedBasedataSize_ + MemUtils::roundDownToOSMemoryPageSize(static_cast<size_t>(minimumLength))};
    return commit(newRuntimeMemorySize);
  } catch (vb::RuntimeError const &e) {
    static_cast<void>(e);
    return false;
  }
  // GCOVR_EXCL_START
  catch (...) {
    UNREACHABLE(return false, "should not throw other except than RuntimeError");
  }
  // GCOVR_EXCL_STOP
}

bool LinearMemoryAllocator::probe(uint32_t const linMemOffset) VB_NOEXCEPT {
  if (virtualMemoryAllocator_.data() == nullptr) {
    return false;
  }

  size_t const linMemCommitedSize{virtualMemoryAllocator_.getCommitedSize() - pagedBasedataSize_};
  if (linMemOffset < linMemCommitedSize) {
    // Already commited
    return true;
  } else if (static_cast<size_t>(linMemOffset) < (static_cast<size_t>(linMemPages_) * static_cast<size_t>(WasmConstants::wasmPageSize))) {
    // Commit new pages
    try {
      size_t const newRuntimeMemorySize{pagedBasedataSize_ + MemUtils::roundUpToOSMemoryPageSize(static_cast<size_t>(linMemOffset) + 1U)};
      return commit(newRuntimeMemorySize);
    } catch (vb::RuntimeError const &e) {
      static_cast<void>(e);
      return false;
    }
    // GCOVR_EXCL_START
    catch (...) {
      UNREACHABLE(return false, "should not throw other except than RuntimeError");
    }
    // GCOVR_EXCL_STOP
  } else {
    return false;
  }
}

void LinearMemoryAllocator::setMemoryLimit(uint64_t const memoryLimit) {
  // Rounded down so we can guarantee to satisfy the limit
  uint64_t const pagedMemoryLimit{MemUtils::roundDownToOSMemoryPageSize(memoryLimit)};

  if (getMemoryUsage() > pagedMemoryLimit) {
    throw RuntimeError(ErrorCode::Limit_too_low__memory_already_in_use);
  } else {
    pagedMemoryLimit_.store(pagedMemoryLimit);
  }
}

bool LinearMemoryAllocator::commit(size_t const newPagedSize) {
  if (newPagedSize > pagedMemoryLimit_) {
    maxDesiredRamOnMemoryExtendFailed_ = newPagedSize;
    return false;
  }
  static_cast<void>(virtualMemoryAllocator_.resize(newPagedSize));
  return true;
}

uint32_t LinearMemoryAllocator::getLinearMemorySize(uint32_t const baseDataSize) const VB_NOEXCEPT {
  uint64_t const linearMemorySize{getMemoryUsage() - MemUtils::roundUpToOSMemoryPageSize(static_cast<size_t>(baseDataSize))};
  return static_cast<uint32_t>(linearMemorySize);
}

} // namespace vb

///
/// @file MemoryHelper.cpp
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
// coverity[autosar_cpp14_a16_2_2_violation]
#include <cstddef>
#include <cstdint>

#include "MemoryHelper.hpp"
#include "Runtime.hpp"

#include "src/config.hpp"
#include "src/core/common/ExtendableMemory.hpp"
#include "src/core/common/WasmConstants.hpp"
#include "src/core/common/basedataoffsets.hpp"
#include "src/core/common/util.hpp"

namespace vb {

#if LINEAR_MEMORY_BOUNDS_CHECKS
uint8_t *MemoryHelper::extensionRequest(uint64_t const minLinMemLengthNeeded, uint32_t const basedataLength,
                                        uint8_t *const originalLinMemBase) VB_NOEXCEPT {
  if ((minLinMemLengthNeeded + basedataLength) > UINT32_MAX) {
    return nullptr;
  }
  uint32_t const minimumLinearMemoryLengthNeeded_32{static_cast<uint32_t>(minLinMemLengthNeeded)};

  Runtime &runtime{*readFromPtr<Runtime *>(pSubI(originalLinMemBase, Basedata::FromEnd::runtimePtrOffset))};
  ExtendableMemory &jobMemory{runtime.jobMemory_};
  uint32_t const wasmMemorySizeInPages{readFromPtr<uint32_t>(pSubI(originalLinMemBase, Basedata::FromEnd::linMemWasmSize))};

  uint32_t const currentActualLinMemSize{readFromPtr<uint32_t>(pSubI(originalLinMemBase, Basedata::FromEnd::actualLinMemByteSize))};
  uint32_t const minimumTotalMemorySizeNeeded{basedataLength + minimumLinearMemoryLengthNeeded_32};

  // Let's round this up to the next even value
  uint32_t const roundedUpMinTotalMemSizeNeeded{(minimumTotalMemorySizeNeeded + 1U) & ~1_U32};

  if (minimumLinearMemoryLengthNeeded_32 <= currentActualLinMemSize) {
    return jobMemory.data();
  } else if (minimumLinearMemoryLengthNeeded_32 > (wasmMemorySizeInPages * WasmConstants::wasmPageSize)) {
    // Any linear memory access above the Wasm is an auto-trap, otherwise access is generally allowed, if we can provide
    // enough memory We return an inverted nullptr as an invalid flag here
    return numToP<uint8_t *>(~static_cast<uintptr_t>(0));
  } else if (jobMemory.size() >= roundedUpMinTotalMemSizeNeeded) {
    // Enough memory is already allocated, simply initialize the new portion which has not yet been used to zero and
    // update the new memory size
    static_cast<void>(std::memset(pAddI(originalLinMemBase, currentActualLinMemSize), 0x00,
                                  static_cast<size_t>(minimumLinearMemoryLengthNeeded_32) - static_cast<size_t>(currentActualLinMemSize)));
    writeToPtr<uint32_t>(pSubI(originalLinMemBase, Basedata::FromEnd::actualLinMemByteSize), minimumLinearMemoryLengthNeeded_32);
    // Memory was not reallocated (and thus definitely not moved) so we return the original base
    return jobMemory.data();
  } else {
    // We need to request more memory, the current ExtendableMemory is not big enough
    // If there is no extension request function set, we trap, since we cannot reallocate it
    if (!jobMemory.hasExtensionRequest()) {
      return nullptr;
    }

    // Call the extension request which will reallocate jobMemory
    jobMemory.extensionRequest(roundedUpMinTotalMemSizeNeeded);

    // Check whether reallocation was successful and there is at least the requested amount of memory available
    if ((jobMemory.data() != nullptr) && (jobMemory.size() >= roundedUpMinTotalMemSizeNeeded)) {
      uint8_t *const newLinearMemoryBasePtr{pAddI(jobMemory.data(), basedataLength)};

      // Initialize the portion of the memory which hasn't been initialized yet to zero and update the new memory size
      static_cast<void>(std::memset(pAddI(newLinearMemoryBasePtr, currentActualLinMemSize), 0x00,
                                    static_cast<size_t>(minimumLinearMemoryLengthNeeded_32) - static_cast<size_t>(currentActualLinMemSize)));
      writeToPtr<uint32_t>(pSubI(newLinearMemoryBasePtr, Basedata::FromEnd::actualLinMemByteSize), minimumLinearMemoryLengthNeeded_32);

      // We then return the new base of the job memory
      return jobMemory.data();
    } else {
      // Will trap, reallocation failed or new memory region is not big enough
      return nullptr;
    }
  }
}
#else
bool MemoryHelper::notifyOfMemoryGrowth(uint8_t *const originalLinMemBase, uint32_t const newLinMemSizeInPages) VB_NOEXCEPT {
  Runtime const &runtime{*readFromPtr<Runtime *>(pSubI(originalLinMemBase, Basedata::FromEnd::runtimePtrOffset))};
  return runtime.extendMemory(newLinMemSizeInPages);
}
#endif

} // namespace vb

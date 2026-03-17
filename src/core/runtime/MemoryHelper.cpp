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
#include "src/core/common/basedataoffsets.hpp"
#include "src/core/common/util.hpp"

namespace vb {

#if LINEAR_MEMORY_BOUNDS_CHECKS
uint8_t *MemoryHelper::extensionRequest(uint64_t const minLinMemLengthNeeded, uint32_t const basedataLength,
                                        uint8_t *const originalLinMemBase) VB_NOEXCEPT {
  // coverity[autosar_cpp14_a16_2_3_violation]
  Runtime const &runtime{*readFromPtr<Runtime *>(pSubI(originalLinMemBase, Basedata::FromEnd::runtimePtrOffset))};
  return runtime.handleExtensionRequest(minLinMemLengthNeeded, basedataLength);
}
#else
bool MemoryHelper::notifyOfMemoryGrowth(uint8_t *const originalLinMemBase, uint32_t const newLinMemSizeInPages) VB_NOEXCEPT {
  // coverity[autosar_cpp14_a16_2_3_violation]
  Runtime const &runtime{*readFromPtr<Runtime *>(pSubI(originalLinMemBase, Basedata::FromEnd::runtimePtrOffset))};
  return runtime.extendMemory(newLinMemSizeInPages);
}
#endif

} // namespace vb

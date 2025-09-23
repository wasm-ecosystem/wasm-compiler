/*
 * Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "common.hpp"

#if !LINEAR_MEMORY_BOUNDS_CHECKS
#include "src/utils/LinearMemoryAllocator.hpp"
#endif

namespace vb {
namespace test {

Runtime createRuntime(ExecutableMemory const &executableMemory) {
#if LINEAR_MEMORY_BOUNDS_CHECKS
  return Runtime{executableMemory,
                 [](vb::ExtendableMemory &currentObject, uint32_t minimumLength, void *const ctx) {
                   static_cast<void>(ctx);
                   if (minimumLength == 0) {
                     free(currentObject.data());
                   } else {
                     minimumLength = std::max(minimumLength, static_cast<uint32_t>(1000U)) * 2U;
                     currentObject.reset(vb::pCast<uint8_t *>(realloc(currentObject.data(), minimumLength)), minimumLength);
                   }
                 },
                 nullptr};
#else
  static LinearMemoryAllocator linearMemoryAllocator;
  return Runtime{executableMemory, linearMemoryAllocator, nullptr};
#endif
}

} // namespace test
} // namespace vb

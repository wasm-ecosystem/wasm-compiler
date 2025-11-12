///
/// @file SignalFunctionWrapper.cpp
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
#include "src/config.hpp"
#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK
#include <cstdint>

#include "LinearMemoryAllocator.hpp"
#include "SignalFunctionWrapper.hpp"

#include "src/core/common/BinaryModule.hpp"
#include "src/core/common/ExtendableMemory.hpp"
#include "src/core/common/TrapCode.hpp"
#include "src/core/common/WasmConstants.hpp"
#include "src/core/common/util.hpp"
#include "src/core/runtime/Runtime.hpp"

namespace vb {

#if !LINEAR_MEMORY_BOUNDS_CHECKS || !ACTIVE_STACK_OVERFLOW_CHECK
thread_local Runtime const *SignalFunctionWrapper::pRuntime_{nullptr};

bool SignalFunctionWrapper::pcInWasmCodeRange(void *const pc) VB_NOEXCEPT {
  uintptr_t const faultAddr{pToNum(pc)};
  Runtime const *const pRuntime{SignalFunctionWrapper::getRuntime()};
  if (pRuntime == nullptr) {
    return false;
  }
  BinaryModule const &binaryModule{pRuntime->getBinaryModule()};
  uintptr_t const codeStartAddr{pToNum(binaryModule.getStartAddress())};
  uintptr_t const codeEndAddr{pToNum(binaryModule.getEndAddress())};
  return (faultAddr >= codeStartAddr) && (faultAddr < codeEndAddr);
}

#endif

#if !LINEAR_MEMORY_BOUNDS_CHECKS
thread_local uint32_t SignalFunctionWrapper::landingPadData_{0U};
#endif

#if !LINEAR_MEMORY_BOUNDS_CHECKS
int64_t SignalFunctionWrapper::getOffsetInLinearMemoryAllocation(void *const addr) VB_NOEXCEPT {
  uintptr_t const faultAddr{pToNum(addr)};
  Runtime const *const pRuntime{SignalFunctionWrapper::getRuntime()};
  if (pRuntime == nullptr) {
    return static_cast<int64_t>(-1);
  }
  uintptr_t const linMemStart{pToNum(pRuntime->unsafe__getLinearMemoryBase())};

  // CAUTION: This currently depends on LinearMemoryAllocator. Other allocators currently cannot be used.
  uintptr_t const guardEnd{linMemStart + WasmConstants::maxLinearMemorySize + LinearMemoryAllocator::offsetGuardRegionSize};

  if ((faultAddr >= linMemStart) && (faultAddr <= guardEnd)) {
    uintptr_t const offset{pToNum(addr) - linMemStart};
    return static_cast<int64_t>(offset);
  } else {
    return static_cast<int64_t>(-1);
  }
}

void SignalFunctionWrapper::probeLinearMemoryOffset() VB_NOEXCEPT {
  if (!pRuntime_->probeLinearMemory(landingPadData_)) {
    // Memory commit was not successful
    pRuntime_->tryTrap(TrapCode::LINMEM_COULDNOTEXTEND);
  }
}
#endif

} // namespace vb
#endif

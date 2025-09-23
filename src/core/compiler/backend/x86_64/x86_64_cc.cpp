///
/// @file x86_64_cc.cpp
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
#include "src/config.hpp"
#ifdef JIT_TARGET_X86_64
#include <array>
#include <cstdint>

#include "x86_64_cc.hpp"

#include "src/core/common/RegPosArr.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_encoding.hpp"

namespace vb {
namespace x86_64 {

namespace WasmABI {

/// @brief Array holding the position of each register in the gpr array
// coverity[autosar_cpp14_a8_5_2_violation]
// coverity[autosar_cpp14_m3_4_1_violation]
static constexpr auto gprRegPos = vb::genPosArr<totalNumRegs>(gpr);
/// @brief Array holding the position of each register in the fpr array
// coverity[autosar_cpp14_a8_5_2_violation]
// coverity[autosar_cpp14_m3_4_1_violation]
static constexpr auto fprRegPos = vb::genPosArr<totalNumRegs>(fpr);

uint32_t getRegPos(REG const reg) VB_NOEXCEPT {
  if (RegUtil::isGPR(reg)) {
    return static_cast<uint32_t>(gprRegPos[static_cast<uint32_t>(reg)]);
  } else {
    return static_cast<uint32_t>(fprRegPos[static_cast<uint32_t>(reg)]);
  }
}

bool isResScratchReg(REG const reg) VB_NOEXCEPT {
  uint32_t const scratchRegStart{RegUtil::isGPR(reg) ? (WasmABI::numGPR - WasmABI::resScratchRegsGPR)
                                                     : (WasmABI::numFPR - WasmABI::resScratchRegsFPR)};
  uint32_t const regPos{getRegPos(reg)};
  return regPos >= scratchRegStart;
}

} // namespace WasmABI

namespace NativeABI {

/// @brief Array holding the position of each register in the volRegs array
// coverity[autosar_cpp14_a8_5_2_violation]
// coverity[autosar_cpp14_m3_4_1_violation]
static constexpr auto volRegPos = vb::genPosArr<totalNumRegs>(volRegs);
/// @brief Array holding the position of each register in the gpParams array
// coverity[autosar_cpp14_a8_5_2_violation]
// coverity[autosar_cpp14_m3_4_1_violation]
static constexpr auto gpParamsPos = vb::genPosArr<totalNumRegs>(gpParams);
/// @brief Array holding the position of each register in the flParams array
// coverity[autosar_cpp14_a8_5_2_violation]
// coverity[autosar_cpp14_m3_4_1_violation]
static constexpr auto fpParamsPos = vb::genPosArr<totalNumRegs>(flParams);

bool isVolatileReg(REG const reg) VB_NOEXCEPT {
  return volRegPos[static_cast<uint32_t>(reg)] != static_cast<uint8_t>(UINT8_MAX);
}

bool canBeParam(REG const reg) VB_NOEXCEPT {
  if (RegUtil::isGPR(reg)) {
    return gpParamsPos[static_cast<uint32_t>(reg)] != static_cast<uint8_t>(UINT8_MAX);
  } else {
    return fpParamsPos[static_cast<uint32_t>(reg)] != static_cast<uint8_t>(UINT8_MAX);
  }
}

uint32_t getNativeParamPos(REG const reg) VB_NOEXCEPT {
  if (RegUtil::isGPR(reg)) {
    return static_cast<uint32_t>(gpParamsPos[static_cast<uint32_t>(reg)]);
  } else {
    return static_cast<uint32_t>(fpParamsPos[static_cast<uint32_t>(reg)]);
  }
}

} // namespace NativeABI

} // namespace x86_64
} // namespace vb

#endif

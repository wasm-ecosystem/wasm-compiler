///
/// @file tricore_cc.cpp
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

#ifdef JIT_TARGET_TRICORE
#include <array>
#include <cassert>
#include <cstdint>

#include "tricore_cc.hpp"

#include "src/core/common/RegPosArr.hpp"
#include "src/core/compiler/backend/tricore/tricore_encoding.hpp"

namespace vb {
namespace tc {

namespace WasmABI {

/// @brief Array holding the position of each register in the dr array
// coverity[autosar_cpp14_a8_5_2_violation]
// coverity[autosar_cpp14_m3_4_1_violation]
static constexpr auto drRegPos = vb::genPosArr<totalNumRegs>(dr);

uint32_t getRegPos(REG const dataReg) VB_NOEXCEPT {
  assert(RegUtil::isDATA(dataReg));
  return static_cast<uint32_t>(drRegPos[static_cast<uint32_t>(dataReg)]);
}

bool isResScratchReg(REG const dataReg) VB_NOEXCEPT {
  assert(RegUtil::isDATA(dataReg));

  uint32_t const regPos{getRegPos(dataReg)};
  return regPos >= scratchRegStart;
}

} // namespace WasmABI

namespace NativeABI {

/// @brief Array holding the position of each register in the paramRegs array
// coverity[autosar_cpp14_a8_5_2_violation]
// coverity[autosar_cpp14_m3_4_1_violation]
static constexpr auto paramsPos = vb::genPosArr<totalNumRegs>(paramRegs);

bool canBeParam(REG const dataReg) VB_NOEXCEPT {
  assert(RegUtil::isDATA(dataReg));
  return paramsPos[static_cast<uint32_t>(dataReg)] != static_cast<uint8_t>(UINT8_MAX);
}

uint32_t getNativeParamPos(REG const reg) VB_NOEXCEPT {
  return static_cast<uint32_t>(paramsPos[static_cast<uint32_t>(reg)]);
}

} // namespace NativeABI

} // namespace tc
} // namespace vb

#endif

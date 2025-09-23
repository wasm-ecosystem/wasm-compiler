///
/// @file CommonRegsPos.cpp
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
#include <array>
#include <cstdint>

#include "src/config.hpp"
#include "src/core/common/RegPosArr.hpp"
#include "src/core/compiler/backend/RegAdapter.hpp"
#include "src/core/compiler/common/Common.hpp"

#ifdef JIT_TARGET_AARCH64
#include "src/core/compiler/backend/aarch64/aarch64_backend.hpp"
#include "src/core/compiler/backend/aarch64/aarch64_cc.hpp"
#include "src/core/compiler/backend/aarch64/aarch64_encoding.hpp"
#endif
#ifdef JIT_TARGET_TRICORE
#include "src/core/compiler/backend/tricore/tricore_backend.hpp"
#include "src/core/compiler/backend/tricore/tricore_cc.hpp"
#include "src/core/compiler/backend/tricore/tricore_encoding.hpp"
#endif
#ifdef JIT_TARGET_X86_64
#include "src/core/compiler/backend/x86_64/x86_64_backend.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_cc.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_encoding.hpp"
#endif

namespace vb {

/// @brief Array holding the position of each register in the callScrRegs array
// coverity[autosar_cpp14_a8_5_2_violation]
// coverity[autosar_cpp14_m3_4_1_violation]
static constexpr auto callSrcRegsPos = genPosArr<NBackend::totalNumRegs>(NBackend::callScrRegs);
bool Common::isCallScrReg(TReg const reg) VB_NOEXCEPT {
  return callSrcRegsPos[static_cast<uint32_t>(reg)] != static_cast<uint8_t>(UINT8_MAX);
}

} // namespace vb

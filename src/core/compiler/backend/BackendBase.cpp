///
/// @file BackendBase.cpp
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

#include "BackendBase.hpp"

#include "src/config.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/common/StackElement.hpp"
#include "src/core/compiler/common/StackType.hpp"
#include "src/core/compiler/common/util.hpp"

/// @brief analyze div rem args
vb::DivRemAnalysisResult vb::analyzeDivRem(StackElement const *const arg0Ptr, StackElement const *const arg1Ptr) VB_NOEXCEPT {
  DivRemAnalysisResult result{false, false};
  if (arg0Ptr->type == StackType::CONSTANT_I32) {
    uint32_t const arg0{arg0Ptr->data.constUnion.u32};
    constexpr uint32_t maxBitSet{1_U32 << 31_U32};
    if ((arg0 != maxBitSet)) {
      result.mustNotBeOverflow = true;
    }
  } else if (arg0Ptr->type == StackType::CONSTANT_I64) {
    uint64_t const arg0{arg0Ptr->data.constUnion.u64};
    constexpr uint64_t maxBitSet{1_U64 << 63_U64};
    if ((arg0 != maxBitSet)) {
      result.mustNotBeOverflow = true;
    }
  } else {
    static_cast<void>(0);
  }

  if (arg1Ptr->type == StackType::CONSTANT_I32) {
    uint32_t const arg1{arg1Ptr->data.constUnion.u32};
    if (arg1 != 0_U32) {
      result.mustNotBeDivZero = true;
    }
    if ((arg1 != static_cast<uint32_t>(-1))) {
      result.mustNotBeOverflow = true;
    }
  } else if (arg1Ptr->type == StackType::CONSTANT_I64) {
    uint64_t const arg1{arg1Ptr->data.constUnion.u64};
    if (arg1 != 0_U64) {
      result.mustNotBeDivZero = true;
    }
    if ((arg1 != static_cast<uint64_t>(-1))) {
      result.mustNotBeOverflow = true;
    }
  } else {
    static_cast<void>(0);
  }
  return result;
}

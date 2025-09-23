///
/// @file StackTop.hpp
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
#ifndef STACKTOP_HPP
#define STACKTOP_HPP

#if (defined __tricore__) || (defined __CPTC__)
namespace vb {
#ifdef __CPTC__
extern "C" {
extern __far uint8_t _lc_ub_ustack; ///< Stack begin
extern __far uint8_t _lc_ue_ustack; ///< Stack end
}

///
/// @brief Get the top of the current stack
///
/// @return Top of the current stack
inline const void *getStackTop() {
  uint8_t *const stackBase = &_lc_ue_ustack;
  static_cast<void>(stackBase);
  return &_lc_ub_ustack;
}

#else
extern "C" {
uint8_t __USTACK;     ///< Stack base
uint8_t __USTACK_TOP; ///< Stack top
}

///
/// @brief Get the top of the current stack
///
/// @return Top of the current stack
inline const void *getStackTop() {
  uint8_t *const stackBase = &__USTACK;
  static_cast<void>(stackBase);
  return &__USTACK_TOP;
}
#endif
} // namespace vb
#else
#include "MemUtils.hpp"
namespace vb {
///
/// @brief Get the top of the current stack
///
/// @return Top of the current stack
inline const void *getStackTop() {
  MemUtils::StackInfo const stackInfo = MemUtils::getStackInfo();
  return stackInfo.stackTop;
}
} // namespace vb

#endif

#endif

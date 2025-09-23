///
/// @file BackendBase.hpp
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
#ifndef SRC_CORE_COMPILER_BACKEND_BACKENDBASE_HPP
#define SRC_CORE_COMPILER_BACKEND_BACKENDBASE_HPP

#include "src/core/compiler/common/StackElement.hpp"

namespace vb {

/// @brief div rem analysis result
struct DivRemAnalysisResult final {
  bool mustNotBeOverflow; ///< ignore overflow check
  bool mustNotBeDivZero;  ///< ignore division by zero check
};

/// @brief analyze div rem args
DivRemAnalysisResult analyzeDivRem(StackElement const *const arg0Ptr, StackElement const *const arg1Ptr) VB_NOEXCEPT;

} // namespace vb

#endif // SRC_CORE_COMPILER_BACKEND_BACKENDBASE_HPP

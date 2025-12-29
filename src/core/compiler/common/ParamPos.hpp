///
/// @file ParamPos.hpp
/// @copyright Copyright (C) 2025 wasm ecosystem contributors
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
#ifndef VB_PARAM_POS_HPP
#define VB_PARAM_POS_HPP
#include "src/core/compiler/backend/RegAdapter.hpp"
namespace vb {
/// @brief Ident if the parameter is passed in register or on stack
struct ParamPos final {
  TReg reg;                   ///< Register used for the parameter
  uint32_t offsetToStackBase; ///< Stack offset used for the parameter
};
} // namespace vb

#endif // VB_PARAM_POS_HPP

///
/// @file FunctionRef.hpp
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
#ifndef FUNCTION_REF_HPP
#define FUNCTION_REF_HPP
#include <functional>
#include <type_traits>
#include <utility>

#include "src/core/common/util.hpp"

namespace vb {
///
/// @brief Base declaration of FunctionRef
///
/// @tparam T Shall be a function type
///
// coverity[autosar_cpp14_a13_3_1_violation]
template <typename T> class FunctionRef;
///
/// @brief Reference to a function type to avoid dynamic memory allocation of std::function
/// @note Caller must ensure memory safety of the function to be referred
///
/// @tparam ReturnType Return type of the function
/// @tparam Arguments Arguments of the function
///
// coverity[autosar_cpp14_a14_1_1_violation]
template <typename ReturnType, typename... Arguments> class FunctionRef<ReturnType(Arguments...)> final {
public:
  ///
  /// @brief Construct a new FunctionRef
  ///
  /// @tparam FunctionType The type of the function to be referred, can be auto deduced
  /// @param function The function to be referred, for example a lambda
  ///
  template <typename FunctionType, class = std::enable_if_t<std::is_convertible<FunctionType &&, std::function<ReturnType(Arguments...)>>::value>>
  // coverity[autosar_cpp14_a7_1_8_violation]
  explicit FunctionRef(FunctionType &&function) VB_NOEXCEPT : function_(pCast<void const *>(&function)),
                                                              executor(&templateExecutor<typename std::remove_reference<FunctionType>::type>) {
  }
  ///
  /// @brief Construct a new FunctionRef from nullptr
  ///
  /// @param function a null pointer
  ///
  // coverity[autosar_cpp14_a13_3_1_violation]
  inline explicit FunctionRef(std::nullptr_t const function) VB_NOEXCEPT : function_(function), executor(nullptr) {
  }
  ///
  /// @brief Construct a new FunctionRef from a raw function pointer
  ///
  /// @param function a raw function pointer
  ///
  // coverity[autosar_cpp14_a13_3_1_violation]
  inline explicit FunctionRef(ReturnType (*function)(Arguments...)) VB_NOEXCEPT : function_(pCast<void const *>(function)),
                                                                                  executor(&templateExecutor<ReturnType(Arguments...)>) {
  }
  ///
  /// @brief call the function reference
  ///
  /// @param args variadic arguments
  /// @return ReturnType The return value
  // coverity[autosar_cpp14_a15_4_4_violation]
  inline ReturnType operator()(Arguments... args) const {
    // NOLINTNEXTLINE(clang-analyzer-core.CallAndMessage)
    return executor(function_, std::forward<Arguments>(args)...);
  }
  ///
  /// @brief Check if the function_ is not nullptr
  ///
  /// @return true
  /// @return false
  ///
  inline bool notNull() const VB_NOEXCEPT {
    return function_ != nullptr;
  }

private:
  void const *function_;                                             ///< Pointer to the referred function
  ReturnType (*executor)(void const *callable, Arguments... params); ///< Pointer to the executor instance

  ///
  /// @brief The executor wrapper to run a function
  ///
  /// @tparam FunctionType The type of the callee function
  /// @param function The callee function
  /// @param params variadic arguments
  /// @return ReturnType The return value
  /// @throws vb::RuntimeError If not enough memory is available
  // coverity[autosar_cpp14_a15_4_4_violation]
  template <typename FunctionType> static ReturnType templateExecutor(void const *const function, Arguments... params) {
    // coverity[autosar_cpp14_a4_5_1_violation]
    return (*pCast<FunctionType *>(pRemoveConst(function)))(std::forward<Arguments>(params)...);
  }
};
} // namespace vb

#endif

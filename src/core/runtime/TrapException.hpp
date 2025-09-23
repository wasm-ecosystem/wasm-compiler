///
/// @file TrapException.hpp
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
#ifndef TRAP_EXCEPTION_HPP
#define TRAP_EXCEPTION_HPP

#include <exception>

#include "src/core/common/TrapCode.hpp"

namespace vb {

///
/// @brief Exception class for WebAssembly traps
///
// coverity[autosar_cpp14_m3_4_1_violation]
class TrapException final : public std::exception {
public:
  ///
  /// @brief Constructor for a TrapException and a given TrapCode
  ///
  /// @param trapCode
  // NOLINTNEXTLINE(readability-redundant-member-init)
  inline explicit TrapException(TrapCode const trapCode) VB_NOEXCEPT : std::exception(), trapCode_(trapCode) {
  }

  ///
  /// @brief Get the stored TrapCode
  ///
  /// @return TrapCode Stored TrapCode
  inline TrapCode getTrapCode() const VB_NOEXCEPT {
    return trapCode_;
  };

  ///
  /// @brief Get a human readable message for this exception and the stored TrapCode
  ///
  /// @return const char*
  inline const char *what() const noexcept final {
    if (static_cast<uint32_t>(trapCode_) >= trapCodeErrorMessages.size()) {
      return "Unknown trap";
    } else {
      return trapCodeErrorMessages[static_cast<uint32_t>(trapCode_)];
    }
  }

  ///
  /// @brief Default copy constructor
  ///
  TrapException(const TrapException &) = default;
  ///
  /// @brief Default move constructor
  ///
  TrapException(TrapException &&) VB_NOEXCEPT = default;
  ///
  /// @brief Default copy operator
  ///
  TrapException &operator=(const TrapException &) & = default;
  ///
  /// @brief Default move operator
  ///
  TrapException &operator=(TrapException &&) &VB_NOEXCEPT = default;
  ///
  /// @brief Default destructor
  ///
  ~TrapException() VB_NOEXCEPT final = default;

private:
  TrapCode trapCode_; ///< Stored TrapCode
};

} // namespace vb

#endif

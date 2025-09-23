///
/// @file TrapCode.hpp
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
#ifndef TRAPCODE_HPP
#define TRAPCODE_HPP

#include <cstdint>

#include "util.hpp"

namespace vb {

///
/// @brief Represents the reason why a trap happened during execution
///
// coverity[autosar_cpp14_a7_2_4_violation] autosar required style is more bug prone
enum class TrapCode : uint32_t {
  /// @brief No trap
  NONE = 0,

  /// @brief Trap due to executing the 'unreachable' Wasm instruction
  UNREACHABLE,
  /// @brief Trap due to calling the imported builtin.trap function
  BUILTIN_TRAP,

  /// @brief Trap when linear memory is accessed outside of the Wasm 'virtual' memory boundaries
  LINMEM_OUTOFBOUNDSACCESS,
  /// @brief Trap when not enough memory could be allocated
  LINMEM_COULDNOTEXTEND,

  /// @brief Trap when an indirect call is executed, but the given index is out of bounds of the Wasm table
  INDIRECTCALL_OUTOFBOUNDS,
  /// @brief Trap when an indirect call is executed, but the given index points to an undefined function or a function
  /// with a different signature
  INDIRECTCALL_WRONGSIG,

  /// @brief Multiplexing entry, that will be converted to either LINKEDMEMORY_NOTLINKED or LINKEDMEMORY_OUTOFBOUNDS
  /// before actually throwing the TrapException
  /// NOTE: This is for internal use, users will never encounter this
  LINKEDMEMORY_MUX,
  /// @brief Trap when the linked memory is accessed, but none is linked
  LINKEDMEMORY_NOTLINKED = LINKEDMEMORY_MUX,
  /// @brief Trap when the linked memory is accessed out of bounds
  LINKEDMEMORY_OUTOFBOUNDS,

  /// @brief Trap when an integer division by zero is executed
  DIV_ZERO,
  /// @brief Trap when a signed integer division is overflowing
  DIV_OVERFLOW,
  /// @brief Trap when a float to int conversion is overflowing
  TRUNC_OVERFLOW,

  /// @brief Trap when the runtime was asynchronously requested to interrupt the execution via
  /// runtime.requestInterrupt()
  RUNTIME_INTERRUPT_REQUESTED,

  /// @brief Trap when the stack fence is breached (e.g. when not enough space is on the stack to execute a native
  /// function) or a stack overflow happens
  STACKFENCEBREACHED,

  ///@brief Called function was not linked
  CALLED_FUNCTION_NOT_LINKED,

  MAX_TRAP_CODE = CALLED_FUNCTION_NOT_LINKED,
};

/// @brief Human readable descriptions for the TrapCode
///
// coverity[autosar_cpp14_a8_5_2_violation]
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr auto trapCodeErrorMessages = make_array(
    /* NONE */ "No trap",

    /* UNREACHABLE */ "Unreachable instruction executed",
    /* BUILTIN_TRAP */ "builtin.trap executed",

    /* LINMEM_OUTOFBOUNDSACCESS */ "Linear memory access out of bounds",
    /* LINMEM_COULDNOTEXTEND */ "Could not extend linear memory",

    /* INDIRECTLY_OUTBOUND */ "Indirect call out of bounds (table)",
    /* INDIRECTCALL_WRONGSIG */ "Indirect call performed with wrong signature",

    /* LINKEDMEMORY_NOTLINKED */ "No memory linked",
    /* LINKEDMEMORY_OUTOFBOUNDS */ "Linked memory access out of bounds",

    /* DIV_ZERO */ "Division by zero",
    /* DIV_OVERFLOW */ "Integer division overflow",
    /* TRUNC_OVERFLOW */ "Float to int conversion overflow",

    /* RUNTIME_INTERRUPT_REQUESTED */ "Runtime interrupt externally triggered",

    /* STACKFENCEBREACHED */ "Stack fence breached",

    /* CALLED_FUNCTION_NOT_LINKED */ "Called function not linked");

} // namespace vb

#endif

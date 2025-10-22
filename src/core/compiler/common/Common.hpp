///
/// @file Common.hpp
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
#ifndef COMMON_HPP
#define COMMON_HPP

#include <array>
#include <cstdint>

#include "BranchCondition.hpp"
#include "ModuleInfo.hpp"
#include "OPCode.hpp"
#include "RegMask.hpp"
#include "StackElement.hpp"
#include "VariableStorage.hpp"

#include "src/core/common/FunctionRef.hpp"
#include "src/core/common/Span.hpp"
#include "src/core/compiler/common/Stack.hpp"

namespace vb {
// coverity[autosar_cpp14_m3_2_3_violation]
class Compiler;

///
/// @brief Arity and operand types for each arithmetic opcode
///
struct ArithArg {
  MachineType arg0Type;   ///< Type of the first operand for the instruction
  MachineType arg1Type;   ///< Type of the second operand for the instruction
  MachineType resultType; ///< Result type for the instruction
  bool commutative;       ///< Whether the operand types are commutative
};

// clang-format off
///
/// @brief ArithArg annotation for each arithmetic WebAssembly opcode
///
constexpr std::array<ArithArg, 128U> arithArgs {{
	/*I32 COMPARISONS*/ 	{/*I32_EQZ */ MachineType::I32, MachineType::INVALID, MachineType::I32, false}, {/*I32_EQ*/ MachineType::I32, MachineType::I32, MachineType::I32, true}, {/*I32_NE*/ MachineType::I32, MachineType::I32, MachineType::I32, true}, {/*I32_LT_S*/ MachineType::I32, MachineType::I32, MachineType::I32, false}, {/*I32_LT_U*/ MachineType::I32, MachineType::I32, MachineType::I32, false}, {/*I32_GT_S*/ MachineType::I32, MachineType::I32, MachineType::I32, false}, {/*I32_GT_U*/ MachineType::I32, MachineType::I32, MachineType::I32, false}, {/*I32_LE_S*/ MachineType::I32, MachineType::I32, MachineType::I32, false}, {/*I32_LE_U*/ MachineType::I32, MachineType::I32, MachineType::I32, false}, {/*I32_GE_S*/ MachineType::I32, MachineType::I32, MachineType::I32, false}, {/*I32_GE_U*/ MachineType::I32, MachineType::I32, MachineType::I32, false},
	/*I64 COMPARISONS*/ 	{/*I64_EQZ */ MachineType::I64, MachineType::INVALID, MachineType::I32, false}, {/*I64_EQ*/ MachineType::I64, MachineType::I64, MachineType::I32, true}, {/*I64_NE*/ MachineType::I64, MachineType::I64, MachineType::I32, true}, {/*I64_LT_S*/ MachineType::I64, MachineType::I64, MachineType::I32, false}, {/*I64_LT_U*/ MachineType::I64, MachineType::I64, MachineType::I32, false}, {/*I64_GT_S*/ MachineType::I64, MachineType::I64, MachineType::I32, false}, {/*I64_GT_U*/ MachineType::I64, MachineType::I64, MachineType::I32, false}, {/*I64_LE_S*/ MachineType::I64, MachineType::I64, MachineType::I32, false}, {/*I64_LE_U*/ MachineType::I64, MachineType::I64, MachineType::I32, false}, {/*I64_GE_S*/ MachineType::I64, MachineType::I64, MachineType::I32, false}, {/*I64_GE_U*/ MachineType::I64, MachineType::I64, MachineType::I32, false},
	/*F32 COMPARISONS*/ 	{/*F32_EQ*/ MachineType::F32, MachineType::F32, MachineType::I32, true}, {/*F32_NE*/ MachineType::F32, MachineType::F32, MachineType::I32, true}, {/*F32_LT*/ MachineType::F32, MachineType::F32, MachineType::I32, false}, {/*F32_GT*/ MachineType::F32, MachineType::F32, MachineType::I32, false}, {/*F32_LE*/ MachineType::F32, MachineType::F32, MachineType::I32, false}, {/*F32_GE*/ MachineType::F32, MachineType::F32, MachineType::I32, false},
	/*F64 COMPARISONS*/ 	{/*F64_EQ*/ MachineType::F64, MachineType::F64, MachineType::I32, true}, {/*F64_NE*/ MachineType::F64, MachineType::F64, MachineType::I32, true}, {/*F64_LT*/ MachineType::F64, MachineType::F64, MachineType::I32, false}, {/*F64_GT*/ MachineType::F64, MachineType::F64, MachineType::I32, false}, {/*F64_LE*/ MachineType::F64, MachineType::F64, MachineType::I32, false}, {/*F64_GE*/ MachineType::F64, MachineType::F64, MachineType::I32, false},
	/*I32 NUMERIC OPS*/ 	{/*I32_CLZ*/ MachineType::I32, MachineType::INVALID, MachineType::I32, false}, {/*I32_CTZ*/ MachineType::I32, MachineType::INVALID, MachineType::I32, false}, {/*I32_POPCNT*/ MachineType::I32, MachineType::INVALID, MachineType::I32, false}, {/*I32_ADD*/ MachineType::I32, MachineType::I32, MachineType::I32, true}, {/*I32_SUB*/ MachineType::I32, MachineType::I32, MachineType::I32, false}, {/*I32_MUL*/ MachineType::I32, MachineType::I32, MachineType::I32, true}, {/*I32_DIV_S*/ MachineType::I32, MachineType::I32, MachineType::I32, false}, {/*I32_DIV_U*/ MachineType::I32, MachineType::I32, MachineType::I32, false}, {/*I32_REM_S*/ MachineType::I32, MachineType::I32, MachineType::I32, false}, {/*I32_REM_U*/ MachineType::I32, MachineType::I32, MachineType::I32, false}, {/*I32_AND*/ MachineType::I32, MachineType::I32, MachineType::I32, true}, {/*I32_OR*/ MachineType::I32, MachineType::I32, MachineType::I32, true}, {/*I32_XOR*/ MachineType::I32, MachineType::I32, MachineType::I32, true}, {/*I32_SHL*/ MachineType::I32, MachineType::I32, MachineType::I32, false}, {/*I32_SHR_S*/ MachineType::I32, MachineType::I32, MachineType::I32, false}, {/*I32_SHR_U*/ MachineType::I32, MachineType::I32, MachineType::I32, false}, {/*I32_ROTL*/ MachineType::I32, MachineType::I32, MachineType::I32, false}, {/*I32_ROTR*/ MachineType::I32, MachineType::I32, MachineType::I32, false},
	/*I64 NUMERIC OPS*/ 	{/*I64_CLZ*/ MachineType::I64, MachineType::INVALID, MachineType::I64, false}, {/*I64_CTZ*/ MachineType::I64, MachineType::INVALID, MachineType::I64, false}, {/*I64_POPCNT*/ MachineType::I64, MachineType::INVALID, MachineType::I64, false}, {/*I64_ADD*/ MachineType::I64, MachineType::I64, MachineType::I64, true}, {/*I64_SUB*/ MachineType::I64, MachineType::I64, MachineType::I64, false}, {/*I64_MUL*/ MachineType::I64, MachineType::I64, MachineType::I64, true}, {/*I64_DIV_S*/ MachineType::I64, MachineType::I64, MachineType::I64, false}, {/*I64_DIV_U*/ MachineType::I64, MachineType::I64, MachineType::I64, false}, {/*I64_REM_S*/ MachineType::I64, MachineType::I64, MachineType::I64, false}, {/*I64_REM_U*/ MachineType::I64, MachineType::I64, MachineType::I64, false}, {/*I64_AND*/ MachineType::I64, MachineType::I64, MachineType::I64, true}, {/*I64_OR*/ MachineType::I64, MachineType::I64, MachineType::I64, true}, {/*I64_XOR*/ MachineType::I64, MachineType::I64, MachineType::I64, true}, {/*I64_SHL*/ MachineType::I64, MachineType::I64, MachineType::I64, false}, {/*I64_SHR_S*/ MachineType::I64, MachineType::I64, MachineType::I64, false}, {/*I64_SHR_U*/ MachineType::I64, MachineType::I64, MachineType::I64, false}, {/*I64_ROTL*/ MachineType::I64, MachineType::I64, MachineType::I64, false}, {/*I64_ROTR*/ MachineType::I64, MachineType::I64, MachineType::I64, false},
	/*F32 NUMERIC OPS*/ 	{/*F32_ABS*/ MachineType::F32, MachineType::INVALID, MachineType::F32, false}, {/*F32_NEG*/ MachineType::F32, MachineType::INVALID, MachineType::F32, false}, {/*F32_CEIL*/ MachineType::F32, MachineType::INVALID, MachineType::F32, false}, {/*F32_FLOOR*/ MachineType::F32, MachineType::INVALID, MachineType::F32, false}, {/*F32_TRUNC*/ MachineType::F32, MachineType::INVALID, MachineType::F32, false}, {/*F32_NEAREST*/ MachineType::F32, MachineType::INVALID, MachineType::F32, false}, {/*F32_SQRT*/ MachineType::F32, MachineType::INVALID, MachineType::F32, false}, {/*F32_ADD*/ MachineType::F32, MachineType::F32, MachineType::F32, true}, {/*F32_SUB*/ MachineType::F32, MachineType::F32, MachineType::F32, false}, {/*F32_MUL*/ MachineType::F32, MachineType::F32, MachineType::F32, true}, {/*F32_DIV*/ MachineType::F32, MachineType::F32, MachineType::F32, false}, {/*F32_MIN*/ MachineType::F32, MachineType::F32, MachineType::F32, true}, {/*F32_MAX*/ MachineType::F32, MachineType::F32, MachineType::F32, true}, {/*F32_COPYSIGN*/ MachineType::F32, MachineType::F32, MachineType::F32, false},
	/*F64 NUMERIC OPS*/ 	{/*F64_ABS*/ MachineType::F64, MachineType::INVALID, MachineType::F64, false}, {/*F64_NEG*/ MachineType::F64, MachineType::INVALID, MachineType::F64, false}, {/*F64_CEIL*/ MachineType::F64, MachineType::INVALID, MachineType::F64, false}, {/*F64_FLOOR*/ MachineType::F64, MachineType::INVALID, MachineType::F64, false}, {/*F64_TRUNC*/ MachineType::F64, MachineType::INVALID, MachineType::F64, false}, {/*F64_NEAREST*/ MachineType::F64, MachineType::INVALID, MachineType::F64, false}, {/*F64_SQRT*/ MachineType::F64, MachineType::INVALID, MachineType::F64, false}, {/*F64_ADD*/ MachineType::F64, MachineType::F64, MachineType::F64, true}, {/*F64_SUB*/ MachineType::F64, MachineType::F64, MachineType::F64, false}, {/*F64_MUL*/ MachineType::F64, MachineType::F64, MachineType::F64, true}, {/*F64_DIV*/ MachineType::F64, MachineType::F64, MachineType::F64, false}, {/*F64_MIN*/ MachineType::F64, MachineType::F64, MachineType::F64, true}, {/*F64_MAX*/ MachineType::F64, MachineType::F64, MachineType::F64, true}, {/*F64_COPYSIGN*/ MachineType::F64, MachineType::F64, MachineType::F64, false},
	/*I32 CONVERSIONS*/ 	{/*I32_WRAP_I64*/ MachineType::I64, MachineType::INVALID, MachineType::I32, false}, {/*I32_TRUNC_F32_S*/ MachineType::F32, MachineType::INVALID, MachineType::I32, false}, {/*I32_TRUNC_F32_U*/ MachineType::F32, MachineType::INVALID, MachineType::I32, false}, {/*I32_TRUNC_F64_U*/ MachineType::F64, MachineType::INVALID, MachineType::I32, false}, {/*I32_TRUNC_F64_U*/ MachineType::F64, MachineType::INVALID, MachineType::I32, false},
	/*I64 CONVERSIONS*/ 	{/*I64_EXTEND_I32_S*/ MachineType::I32, MachineType::INVALID, MachineType::I64, false}, {/*I64_EXTEND_I32_U*/ MachineType::I32, MachineType::INVALID, MachineType::I64, false}, {/*I64_TRUNC_F32_S*/ MachineType::F32, MachineType::INVALID, MachineType::I64, false}, {/*I64_TRUNC_F32_U*/ MachineType::F32, MachineType::INVALID, MachineType::I64, false}, {/*I64_TRUNC_F64_U*/ MachineType::F64, MachineType::INVALID, MachineType::I64, false}, {/*I64_TRUNC_F64_U*/ MachineType::F64, MachineType::INVALID, MachineType::I64, false},
	/*F32 CONVERSIONS*/ 	{/*F32_CONVERT_I32_S*/ MachineType::I32, MachineType::INVALID, MachineType::F32, false}, {/*F32_CONVERT_I32_U*/ MachineType::I32, MachineType::INVALID, MachineType::F32, false}, {/*F32_CONVERT_I64_S*/ MachineType::I64, MachineType::INVALID, MachineType::F32, false}, {/*F32_CONVERT_I64_U*/ MachineType::I64, MachineType::INVALID, MachineType::F32, false}, {/*F32_DEMOTE_F64*/ MachineType::F64, MachineType::INVALID, MachineType::F32, false},
	/*F64 CONVERSIONS*/ 	{/*F64_CONVERT_I32_S*/ MachineType::I32, MachineType::INVALID, MachineType::F64, false}, {/*F64_CONVERT_I32_U*/ MachineType::I32, MachineType::INVALID, MachineType::F64, false}, {/*F64_CONVERT_I64_S*/ MachineType::I64, MachineType::INVALID, MachineType::F64, false}, {/*F64_CONVERT_I64_U*/ MachineType::I64, MachineType::INVALID, MachineType::F64, false}, {/*F64_PROMOTE_F32*/ MachineType::F32, MachineType::INVALID, MachineType::F64, false},
	/*REINTERPRETATIONS*/ 	{/*I32_REINTERPRET_F32*/ MachineType::F32, MachineType::INVALID, MachineType::I32, false}, {/*I64_REINTERPRET_F64*/ MachineType::F64, MachineType::INVALID, MachineType::I64, false}, {/*F32_REINTERPRET_I32*/ MachineType::I32, MachineType::INVALID, MachineType::F32, false}, {/*F64_REINTERPRET_I64*/ MachineType::I64, MachineType::INVALID, MachineType::F64, false},
	/*SIGN EXTENSION OPS*/ 	{/*I32_EXTEND8_S*/ MachineType::I32, MachineType::INVALID, MachineType::I32, false}, {/*I32_EXTEND16_S*/ MachineType::I32, MachineType::INVALID, MachineType::I32, false}, {/*I64_EXTEND8_S*/ MachineType::I64, MachineType::INVALID, MachineType::I64, false}, {/*I64_EXTEND16_S*/ MachineType::I64, MachineType::INVALID, MachineType::I64, false},{/*I64_EXTEND32_S*/ MachineType::I64, MachineType::INVALID, MachineType::I64, false},
}};
// clang-format on

// clang-format off
///
/// @brief Whether an opcode can trap
///
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr std::array<bool, 128> opcodeMightTrap {{
	/*I32 COMPARISONS*/ 	/*I32_EQZ */ false, /*I32_EQ*/ false, /*I32_NE*/ false, /*I32_LT_S*/ false, /*I32_LT_U*/ false, /*I32_GT_S*/ false, /*I32_GT_U*/ false, /*I32_LE_S*/ false, /*I32_LE_U*/ false, /*I32_GE_S*/ false, /*I32_GE_U*/ false,
	/*I64 COMPARISONS*/ 	/*I64_EQZ */ false, /*I64_EQ*/ false, /*I64_NE*/ false, /*I64_LT_S*/ false, /*I64_LT_U*/ false, /*I64_GT_S*/ false, /*I64_GT_U*/ false, /*I64_LE_S*/ false, /*I64_LE_U*/ false, /*I64_GE_S*/ false, /*I64_GE_U*/ false,
	/*F32 COMPARISONS*/ 	/*F32_EQ*/ false, /*F32_NE*/ false, /*F32_LT*/ false, /*F32_GT*/ false, /*F32_LE*/ false, /*F32_GE*/ false,
	/*F64 COMPARISONS*/ 	/*F64_EQ*/ false, /*F64_NE*/ false, /*F64_LT*/ false, /*F64_GT*/ false, /*F64_LE*/ false, /*F64_GE*/ false,
	/*I32 NUMERIC OPS*/ 	/*I32_CLZ*/ false, /*I32_CTZ*/ false, /*I32_POPCNT*/ false, /*I32_ADD*/ false, /*I32_SUB*/ false, /*I32_MUL*/ false, /*I32_DIV_S*/ true, /*I32_DIV_U*/ true, /*I32_REM_S*/ true, /*I32_REM_U*/ true, /*I32_AND*/ false, /*I32_OR*/ false, /*I32_XOR*/ false, /*I32_SHL*/ false, /*I32_SHR_S*/ false, /*I32_SHR_U*/ false, /*I32_ROTL*/ false, /*I32_ROTR*/ false,
	/*I64 NUMERIC OPS*/ 	/*I64_CLZ*/ false, /*I64_CTZ*/ false, /*I64_POPCNT*/ false, /*I64_ADD*/ false, /*I64_SUB*/ false, /*I64_MUL*/ false, /*I64_DIV_S*/ true, /*I64_DIV_U*/ true, /*I64_REM_S*/ true, /*I64_REM_U*/ true, /*I64_AND*/ false, /*I64_OR*/ false, /*I64_XOR*/ false, /*I64_SHL*/ false, /*I64_SHR_S*/ false, /*I64_SHR_U*/ false, /*I64_ROTL*/ false, /*I64_ROTR*/ false,
	/*F32 NUMERIC OPS*/ 	/*F32_ABS*/ false, /*F32_NEG*/ false, /*F32_CEIL*/ false, /*F32_FLOOR*/ false, /*F32_TRUNC*/ false, /*F32_NEAREST*/ false, /*F32_SQRT*/ false, /*F32_ADD*/ false, /*F32_SUB*/ false, /*F32_MUL*/ false, /*F32_DIV*/ false, /*F32_MIN*/ false, /*F32_MAX*/ false, /*F32_COPYSIGN*/ false,
	/*F64 NUMERIC OPS*/ 	/*F64_ABS*/ false, /*F64_NEG*/ false, /*F64_CEIL*/ false, /*F64_FLOOR*/ false, /*F64_TRUNC*/ false, /*F64_NEAREST*/ false, /*F64_SQRT*/ false, /*F64_ADD*/ false, /*F64_SUB*/ false, /*F64_MUL*/ false, /*F64_DIV*/ false, /*F64_MIN*/ false, /*F64_MAX*/ false, /*F64_COPYSIGN*/ false,
	/*I32 CONVERSIONS*/ 	/*I32_WRAP_I64*/ false, /*I32_TRUNC_F32_S*/ true, /*I32_TRUNC_F32_U*/ true, /*I32_TRUNC_F64_U*/ true, /*I32_TRUNC_F64_U*/ true,
	/*I64 CONVERSIONS*/ 	/*I64_EXTEND_I32_S*/ false, /*I64_EXTEND_I32_U*/ false, /*I64_TRUNC_F32_S*/ true, /*I64_TRUNC_F32_U*/ true, /*I64_TRUNC_F64_S*/ true, /*I64_TRUNC_F64_U*/ true,
	/*F32 CONVERSIONS*/ 	/*F32_CONVERT_I32_S*/ false, /*F32_CONVERT_I32_U*/ false, /*F32_CONVERT_I64_S*/ false, /*F32_CONVERT_I64_U*/ false, /*F32_DEMOTE_F64*/ false,
	/*F64 CONVERSIONS*/ 	/*F64_CONVERT_I32_S*/ false, /*F64_CONVERT_I32_U*/ false, /*F64_CONVERT_I64_S*/ false, /*F64_CONVERT_I64_U*/ false, /*F64_PROMOTE_F32*/ false,
	/*REINTERPRETATIONS*/ 	/*I32_REINTERPRET_F32*/ false, /*I64_REINTERPRET_F64*/ false, /*F32_REINTERPRET_I32*/ false, /*F64_REINTERPRET_I64*/ false,
	/*SIGN EXTENSION OPS*/ 	/*I32_EXTEND8_S*/ false, /*I32_EXTEND16_S*/ false, /*I64_EXTEND8_S*/ false, /*I64_EXTEND16_S*/ false, /*I64_EXTEND32_S*/ false,
}};
// clang-format on

static_assert(arithArgs.size() == (static_cast<uint32_t>(OPCode::I64_EXTEND32_S) - static_cast<uint32_t>(OPCode::I32_EQZ) + 1U),
              "ArithArgs array wrong size");

/// @brief Get the ArithArg structure for a given instruction
/// @param instruction Instruction for which to get the ArithArg structure
inline constexpr ArithArg const &getArithArgs(OPCode const instruction) VB_NOEXCEPT {
  // GCOVR_EXCL_START
  assert((instruction >= OPCode::I32_EQZ) && "Instruction out of range for arith args");
  // GCOVR_EXCL_STOP
  return arithArgs[static_cast<uint32_t>(instruction) - static_cast<uint32_t>(OPCode::I32_EQZ)];
}

/// @brief Get the lead result type for a given memory load instruction
/// @param instruction memory load instruction
inline MachineType getLoadResultType(OPCode const instruction) VB_NOEXCEPT {
  // coverity[autosar_cpp14_a8_5_2_violation]
  constexpr auto loadResultTypes =
      make_array(MachineType::I32, MachineType::I64, MachineType::F32, MachineType::F64, MachineType::I32, MachineType::I32, MachineType::I32,
                 MachineType::I32, MachineType::I64, MachineType::I64, MachineType::I64, MachineType::I64, MachineType::I64, MachineType::I64);
  assert((instruction >= OPCode::I32_LOAD) && (instruction <= OPCode::I64_LOAD32_U) && "Instruction out of range for lead result type");
  return loadResultTypes[static_cast<uint32_t>(instruction) - static_cast<uint32_t>(OPCode::I32_LOAD)];
}
///
/// @brief Candidate structure for register allocation
///
class RegAllocCandidate final {
public:
  TReg reg = TReg::NONE;       ///< Target Register
  bool currentlyInUse = false; ///< Whether the provided location represented by the StackElement is currently in use
                               ///< and should be spilled before being clobbered
};

///
/// @brief Description of a StackElement that is guaranteed to be in a register
//
class RegElement final {
public:
  StackElement elem = StackElement::invalid(); ///<  Element
  TReg reg = TReg::NONE;                       ///< Underlying register
};

///
/// @brief Common utility functions both the frontend and backend can use
///
class Common final {
public:
  ///
  /// @brief Constructor
  ///
  /// @param compiler Reference to the Compiler instance
  inline explicit Common(Compiler &compiler) VB_NOEXCEPT : compiler_{compiler}, hasPendingSideEffectInstructions_{false} {};

  ///
  /// @brief Get the current maximum used stack frame position of any StackElement currently on the stack
  ///
  /// @return uint32_t Current maximum used stack frame position
  uint32_t getCurrentMaximumUsedStackFramePosition() const VB_NOEXCEPT;

  ///
  /// @brief Checks whether a StackElement represents a writable scratch register
  ///
  /// Writable scratch register are StackElements of type SCRATCHREGISTER that do not appear on the stack or only appear
  /// once
  ///
  /// @param pElem StackElement to check
  /// @return bool Whether this StackElement represents a writable scratch register
  bool isWritableScratchReg(StackElement const *const pElem) const VB_NOEXCEPT;

  ///
  /// @brief Spill a StackElement from the compiler stack
  ///
  /// This will create a copy of the data this StackElement points to without modifying the given source - either in
  /// another scratch register or on the stack - if it is currently present in the topmost frame on the stack and will
  /// replace all corresponding StackElements on the stack with the new representation. Does nothing if no element
  /// corresponding to the given element is present in the current stack frame This can optionally exclude a given
  /// number of StackElements denoted by the top and bottom of the excluded zone on the stack
  ///
  /// CAUTION: Undefined behavior if either pExcludedZoneBottom or pExcludedZoneTop do not point to StackElements on the
  /// compiler stack
  ///
  /// @param source StackElement to spill
  /// @param protRegs Protected register mask, i.e. which registers this operation is not allowed to use
  /// @param forceToStack Whether to disallow spilling the data to another scratch register
  /// @param presFlags Whether to forcefully preserve CPU flags (e.g. for usage between a comparison and a conditional
  /// branch)
  /// @param pExcludedZoneBottom Bottom of the excluded zone (inclusive of this iterator) where no modifications/spills
  /// will be performed
  /// @param pExcludedZoneTop Top of the excluded zone (not inclusive of this iterator) where no modifications/spills will be
  /// performed
  void spillFromStackImpl(StackElement const &source, RegMask const protRegs, bool const forceToStack, bool const presFlags,
                          Stack::iterator const pExcludedZoneBottom = Stack::iterator(),
                          Stack::iterator const pExcludedZoneTop = Stack::iterator()) const;

  ///
  /// @brief Get the base (iterator to the bottommost StackElement) of the current frame
  /// The frame can be either defined by a structural instruction (BLOCK, LOOP, IFBLOCK) or the function frame
  ///
  /// @return Stack::iterator Iterator to the base of the current frame (NOTE: Might be an invalid pointer)
  Stack::iterator getCurrentFrameBase() const VB_NOEXCEPT;

  /// @brief Find the base (iterator to the bottommost StackElement) of a valent block starting at the given position
  ///
  /// @param belowIt iterator that represents the next element of the first valent block.
  /// @return Stack::iterator Base of the valent block
  /// @throws ValidationException Stack frame underflow
  Stack::iterator findBaseOfValentBlockBelow(Stack::iterator const belowIt) const VB_NOEXCEPT;

  /// @brief Find the base (iterator to the bottommost StackElement) of a valent block
  /// @param rootNode iterator that represents the root node of the valent block.
  /// @return Stack::iterator Base of the valent block
  static Stack::iterator findBaseOfValentBlock(Stack::iterator const rootNode) VB_NOEXCEPT;

  /// @brief Skip n valent blocks from stack top
  /// @param count Number of valent blocks to skip
  Stack::iterator skipValentBlock(uint32_t const count) const VB_NOEXCEPT;

  ///
  /// @brief Condense multiple valent blocks and return the base of the last (lowest) one
  ///
  /// @param belowIt Iterator that represents the next element of the first valent block.
  /// @param valentBlockCount Number of valent blocks to condense, must be greater than zero
  /// @return Base of the last (lowest) valent block
  Stack::iterator condenseMultipleValentBlocksBelow(Stack::iterator const belowIt, uint32_t const valentBlockCount) const;

  ///
  /// @brief Condense multiple valent blocks with targetHint and return the base of the last (lowest) one
  ///
  /// @param belowIt Iterator that represents the next element of the first valent block.
  /// @param sigIndex Index of the signature
  /// @param isLoop Whether this is a LOOP
  /// @return Base of the last (lowest) valent block
  Stack::iterator condenseMultipleValentBlocksWithTargetHintBelow(Stack::iterator const belowIt, uint32_t const sigIndex,
                                                                  bool const isLoop = false) const;

  ///
  /// @brief Result of condensation of a comparison
  ///
  // coverity[autosar_cpp14_a11_0_1_violation]
  struct ConditionResult final {
    Stack::iterator base; ///< Where on stack the base of this condensed condition is located (for truncation)
    BC branchCond{};      ///< If this comparison is used as input for a conditional branch, what the correct branch condition
                          ///< is to branch
  };

  ///
  /// @brief Resolve/condense a valent block that will be used as input for a conditional branch. This will emit machine
  /// code and set the corresponding CPU flags.
  ///
  /// @param belowIt iterator that represents the next element of the first valent block.
  /// @return ConditionResult Result of the comparison with a pointer to the bottommost element of this valent block and
  /// the BranchCondition that should be used to positively branch on this condition
  BC condenseComparisonBelow(Stack::iterator const belowIt) const;

  ///
  /// @brief Resolve/condense a valent block by emitting machine code and reducing it into a single StackElement on the
  /// stack that represents the equivalent data of this valent block once all sub-valent blocks have been resolved
  /// accordingly
  ///
  /// This will leave the StackElement representing the result of this condensation operation in the bottommost
  /// StackElement on the stack, setting all others to be skipped when traversing. After this function returns, the
  /// stack can be truncated to above this element so this element, i.e. the result, is still kept on the stack
  ///
  /// @param belowIt iterator that represents the next element of the first valent block.
  /// @param enforcedTarget Optional pointer (can be nullptr) to a StackElement representing a location (e.g. a local variable) where the result of
  /// this valent block condensation should be stored
  /// @return Stack::iterator Iterator to the bottommost element, i.e. the result of this StackElement
  Stack::iterator condenseValentBlockBelow(Stack::iterator const belowIt, StackElement const *const enforcedTarget = nullptr) const;

  ///
  /// @brief Condense the current valent block on the stack if it contains an instruction with side effects
  ///
  /// Scans the current valent block for instructions with side effects and condenses the block if any are found.
  /// This ensures that instructions with side effects are executed in the correct order.
  ///
  void condenseCurrentValentBlockIfSideEffect();

  ///
  /// @brief Condense all instructions with side effects from the current frame base to the frame base
  ///
  /// Iterates through the stack from the frame base to the top, condensing any valent blocks
  /// that contain instructions with side effects to ensure proper execution order.
  ///
  void condenseSideEffectInstructionToFrameBase();

  ///
  /// @brief Condense all instructions with side effects from the current frame base to the specified position
  ///
  /// @param belowIt Upper bound iterator that limits the search range
  ///
  /// Iterates through the stack from the frame base to the specified position,
  /// condensing any valent blocks that contain instructions with side effects.
  ///
  void condenseSideEffectInstructionToFrameBase(Stack::iterator const belowIt);

  ///
  /// @brief skip a given number of condensed valent blocks and condense the side effect instruction that blew the valent block
  /// @param count Number of condensed valent blocks to skip
  void condenseSideEffectInstructionBlewValentBlock(uint32_t const count);

  ///
  /// @brief For multiple condensed return values, load it in the proper return value location
  ///
  /// @param returnValuesBase Iterator to the start (bottommost element) of the condensed valent block that should be loaded
  /// @param numReturnValues number of return values
  /// @param targetBlockElem Optional pointer (can be nullptr) to a StackElement representing a structural block, nullptr means a function
  /// @param presFlags Whether to forcefully preserve CPU flags (e.g. for usage between a comparison and a conditional branch)
  ///
  void loadReturnValues(Stack::iterator const returnValuesBase, uint32_t const numReturnValues, StackElement const *const targetBlockElem = nullptr,
                        bool const presFlags = false) const;

  ///
  /// @brief Pop return value elements of the stack, and remove the reference
  ///
  /// @param returnValuesBase Pointer to the start (bottommost element) of the condensed valent block that should be removed
  /// @param numReturnValues number of return values
  ///
  void popReturnValueElems(Stack::iterator const returnValuesBase, uint32_t const numReturnValues) const VB_NOEXCEPT;

  ///
  /// @brief Condense, drop/discard the result of the topmost valent block on the stack
  /// If current block is unreachable, no deferredAction can push into stack, only dummy variables and block elements.
  /// If reachable, since validation has done independently, drop should never reach function base(always has element to drop)
  void dropValentBlock() const VB_NOEXCEPT;

  ///
  /// @brief Add a reference to the given StackElement in the index and link it properly on the stack linked list
  ///
  /// @param element Element for which to add a reference to the index if needed
  void addReference(Stack::iterator const element) const VB_NOEXCEPT;

  ///
  /// @brief Remove a reference to the given StackElement from the index and unlink it properly from the stack linked
  /// list
  ///
  /// @param element Element for which to remove the reference from the index if needed
  void removeReference(Stack::iterator const element) const VB_NOEXCEPT;

  ///
  /// @brief Push a given StackElement onto the stack and add it to the index
  ///
  /// @param element Element which to push to the stack and for which to add a reference to the index if needed
  /// @throws std::range_error If not enough memory is available
  void pushAndUpdateReference(StackElement const &element) const;

  ///
  /// @brief Pop the topmost StackElement off the stack and remove it from the index
  ///
  void popAndUpdateReference() const VB_NOEXCEPT;

  ///
  /// @brief Replace a StackElement that is currently on the stack with another StackElement, remove the old one from
  /// the index and add the new one to the index
  ///
  /// NOTE: Undefined behavior if the originalPtr does not point to a StackElement on the stack
  ///
  /// @param originalElement Iterator on the stack which will be replaced by the new element
  /// @param newElement StackElement which will be put onto the stack as replacement for the original StackElement
  void replaceAndUpdateReference(Stack::iterator const originalElement, StackElement const &newElement) const VB_NOEXCEPT;

  ///
  /// @brief Check whether the execution of a given OPCode can lead to a WebAssembly trap
  ///
  /// @param opcode OPCode to check
  /// @return bool Whether the OPCode can lead to a WebAssembly trap
  static inline bool opcodeCanTrap(OPCode const opcode) VB_NOEXCEPT {
    assert(opcode >= OPCode::I32_EQZ && opcode <= OPCode::I64_EXTEND32_S);
    return opcodeMightTrap[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_EQZ)];
  }

  ///
  /// @brief Check if a given enforced target is only among the input operands and can thus be assumed to be writable
  /// without destroying any relevant/important information
  ///
  /// @param args argument list
  /// @param enforcedTarget Enforced target to compare
  /// @return bool Whether this enforced target is only among the (up to 2) input operands
  bool checkIfEnforcedTargetIsOnlyInArgs(Span<Stack::iterator> const &args, StackElement const *const enforcedTarget) const VB_NOEXCEPT;

  ///
  /// @brief Result of liftToRegInPlaceProt
  ///
  struct LiftedReg final {
    TReg reg;      ///< the lifted register
    bool writable; ///< whether the register is writable
  };

  ///
  /// @brief Moves a given StackElement to a register, updates it in place, updates references and returns the target
  /// register
  ///
  /// Returns the input if the input is already in a register and fulfills the writability requirement
  ///
  /// @param element Representation of the input element
  /// @param targetNeedsToBeWritable Whether the target needs to be written to or is only read from (i.e. local
  /// variables can be directly used if they are only read from)
  /// @param targetHint A pointer to a StackElement that is preferred as a target if it is corresponding to a register
  /// of the same MachineType as the input element and it is not forbidden by protRegs (pass nullptr if no targetHint)
  /// @param regAllocTracker A tracker to track the protected register mask which prevents the registers which
  /// correspond to the set bits in the mask from being modified
  /// @return lifted register and whether the register is writable
  LiftedReg liftToRegInPlaceProt(StackElement &element, bool const targetNeedsToBeWritable, StackElement const *const targetHint,
                                 RegAllocTracker &regAllocTracker) const;

  ///
  /// @brief Moves a given StackElement to a register, updates it in place, updates references and returns the target
  /// register
  ///
  /// Returns the input if the input is already in a register and fulfills the writability requirement
  ///
  /// @param element Representation of the input element
  /// @param targetNeedsToBeWritable Whether the target needs to be written to or is only read from (i.e. local
  /// variables can be directly used if they are only read from)
  /// @param regAllocTracker A tracker to track the protected register mask which prevents the registers which
  /// correspond to the set bits in the mask from being modified
  /// @return lifted register and whether the register is writable
  inline LiftedReg liftToRegInPlaceProt(StackElement &element, bool const targetNeedsToBeWritable, RegAllocTracker &regAllocTracker) const {
    return liftToRegInPlaceProt(element, targetNeedsToBeWritable, nullptr, regAllocTracker);
  }

  ///
  /// @brief Request a scratch register element for the given MachineType
  ///
  /// @param type MachineType the register should hold
  /// @param targetHint A pointer to a StackElement that is preferred as a target if it is corresponding to a register
  /// of the requested MachineType and it is not forbidden by the frMask (pass nullptr if no targetHint)
  /// @param regAllocTracker A tracker to track the protected register mask which prevents the registers which
  /// correspond to the set bits in the mask from being modified
  /// @param presFlags Whether to guarantee preservation of the CPU status flags (i.e. compare flags). This is important
  /// if this function is to be used between a CMP and a Jcc instruction
  /// @return StackElement Representation of the target register
  RegElement reqScratchRegProt(MachineType const type, StackElement const *const targetHint, RegAllocTracker &regAllocTracker,
                               bool const presFlags) const;

  ///
  /// @brief Request a scratch register element for the given MachineType
  ///
  /// @param type MachineType the register should hold
  /// @param regAllocTracker A tracker to track the protected register mask which prevents the registers which
  /// correspond to the set bits in the mask from being modified
  /// @param presFlags Whether to guarantee preservation of the CPU status flags (i.e. compare flags). This is important
  /// if this function is to be used between a CMP and a Jcc instruction
  /// @return StackElement Representation of the target register
  RegElement reqScratchRegProt(MachineType const type, RegAllocTracker &regAllocTracker, bool const presFlags) const;
  ///
  /// @brief Request a free scratch register for the given MachineType
  ///
  /// @param type MachineType the register should hold
  /// @param regAllocTracker A tracker to track the protected register mask which prevents the registers which
  /// correspond to the set bits in the mask from being modified
  /// @return Target register
  TReg reqFreeScratchRegProt(MachineType const type, RegAllocTracker &regAllocTracker) const VB_NOEXCEPT;

  ///
  /// @brief during function call, move all locals from their register to stack if they is currently stored in a
  /// register
  /// @param onlySaveVolatileReg if true, only save volatile registers, if false, save both volatile and non-volatile
  /// registers
  /// @return RegMask for moved regs.
  RegMask saveLocalsAndParamsForFuncCall(bool const onlySaveVolatileReg) const;

  /// @brief force initialized local with zero
  /// @param localIdx local index
  void initializedLocal(uint32_t const localIdx) const;

  /// @brief force to initialize all local with zero
  void initializedAllLocal() const;

  ///
  /// @brief Move local from stack to its register if it has an assigned register
  ///
  /// @param localIdx Index of the local
  /// @param isReachable if isReachable is false, compiler will not generate real code.
  void recoverLocalToReg(uint32_t const localIdx, bool const isReachable) const;

  ///
  /// @brief Unify local status to STACK_REG before/after branching
  ///
  /// @param localIdx Index of the local
  /// @param isReachable if isReachable is false, compiler will not generate real code.
  void recoverAllLocalsToRegForBranch(uint32_t const localIdx, bool const isReachable) const;

  ///
  /// @brief Optimized move local from stack to its register for `local.set` and `local.tee`.
  /// if local never used in stack before, it will spill it to register, otherwise we just force use it as register.
  ///
  /// @param localIdx Index of the local
  void prepareLocalForSetValue(uint32_t const localIdx) const;

  ///
  /// @brief Move all locals from stack to its register if it has an assigned register
  ///
  /// @param isReachable if isReachable is false, compiler will not generate real code.
  void recoverAllLocalsToRegBranch(bool const isReachable) const;

  ///
  /// @brief Move global from link data to reg
  void recoverGlobalsToRegs() const;

  /// @brief Move global from reg to link data
  void moveGlobalsToLinkData() const;

  ///
  /// @brief Produces a function that traps from native and wasm JIT code
  ///
  /// This can be used from native functions to forcefully unwind the stack and throw an exception, execution of the
  /// wrapper will not return. This adheres to the native calling convention and has produces a function with the
  /// signature void (linearMemoryAddress,trapCode);
  void emitGenericTrapHandler();

  /// @brief if local reg is still available, we can use reg as source
  ///
  /// @param elem Element for which to get the optimized storage location
  /// @param availableLocalsRegMask RegMask describing which registers still have locals in them
  /// @return Optimized storage
  VariableStorage getOptimizedSourceStorage(StackElement const &elem, RegMask const availableLocalsRegMask) const VB_NOEXCEPT;

  /// @brief control flow may jump from other basis block.
  /// for END of BLOCK, compiler has processed all potential branches.
  /// for LOOP, compiler does process branches. All potential branches is based on current state. It would be helpful
  /// when the state transition function is monotonic. for ELSE(beginning of ELSE), there is only one potential branch,
  /// compiler should fallback to the beginning of IF. for END of IF, compiler has processed all potential branches.
  /// @param isReachable if isReachable is false, compiler will not generate real code. it can reduce code size for
  /// unreachable.
  /// @param finishedBlock block which frontend are merging.
  void emitBranchMergePoint(bool const isReachable, StackElement const *const finishedBlock) const;

  /// @brief control flow may jump to other basis block.
  /// for BR / BR_IF / ELSE, compiler has known the target position.
  /// @param isReachable if isReachable is false, compiler will not generate real code. it can reduce code size for
  /// unreachable.
  /// @param targetBlock branch target block
  void emitBranchDivergePoint(bool const isReachable, Stack::iterator const targetBlock) const;

  /// @brief control flow may jump to other basis block.
  /// for BR / BR_IF / ELSE, compiler has known the target position.
  /// @param isReachable if isReachable is false, compiler will not generate real code. it can reduce code size for
  /// unreachable.
  /// @param targetBlockNum count of target block
  /// @param targetBlockFunc function to get target block
  void emitBranchDivergePoint(bool const isReachable, uint32_t const targetBlockNum, FunctionRef<Stack::iterator()> const &targetBlockFunc) const;

#if ENABLE_EXTENSIONS
  ///
  /// @brief Get the number of used/active TempStack slots on the runtime stack
  ///
  /// @return Number of used TempStack slots
  uint32_t getNumUsedTempStackSlots() const VB_NOEXCEPT;
#endif

  ///
  /// @brief Find a free temp stack slot
  ///
  /// @param slotSize current only 8 is possible
  /// @return uint32_t offset on stack
  ///
  uint32_t findFreeTempStackSlot(uint32_t const slotSize) const VB_NOEXCEPT;

  ///
  /// @brief Get the width in bytes of all the return values that are passed on the stack for a function with a
  /// type/signature index
  ///
  /// @param sigIndex Signature or type index of the function
  /// @param isLoop Whether this is a LOOP.
  /// @return uint32_t Width in bytes of all the return values passed on the stack for this function signature
  uint32_t getStackReturnValueWidth(uint32_t const sigIndex, bool const isLoop = false) const VB_NOEXCEPT;

  ///
  /// @brief emit compile time optimized IsFunctionLinked
  ///
  /// @param fncTableIdxElementPtr the stack iterator represent the function table index
  void emitIsFunctionLinkedCompileTimeOpt(Stack::iterator const fncTableIdxElementPtr) VB_NOEXCEPT;

  ///
  /// @brief Checks if two StackElement pointers represent values stored in the same register
  ///
  /// @param lhs First StackElement pointer to compare
  /// @param rhs Second StackElement pointer to compare
  /// @param requestWasmTypeMatch Whether the MachineType of both elements should match
  /// @return bool Whether both elements are stored in the same register
  bool inSameReg(StackElement const *const lhs, StackElement const *const rhs, bool const requestWasmTypeMatch) const VB_NOEXCEPT;

  ///
  /// @brief By the input stack element, usually target hint or temp result, generate a StackElement to hold the result using the target hint
  /// storage
  /// @param stackElement
  /// @param type result wasm type
  StackElement getResultStackElement(StackElement const *const stackElement, MachineType const type) const VB_NOEXCEPT;

  ///
  /// @brief Get the arity of a WebAssembly opcode
  ///
  /// @param opcode WebAssembly opcode
  /// @return uint32_t Arity
  static uint32_t getArithArity(OPCode const opcode) VB_NOEXCEPT;

  /// @brief Push a deferred action onto the stack and modify the hasPendingSideEffectInstructions_
  /// @param deferredAction StackElement that represents a deferred action
  /// @return Stack::iterator Iterator to the pushed deferred action
  Stack::iterator pushDeferredAction(StackElement const &deferredAction);

  /// @brief Push operands of a deferred action onto the stack and modify the sibling links
  /// @param arg Operand stack element
  /// @return Stack::iterator Iterator to the pushed operand
  Stack::iterator pushOperandsToStack(StackElement const &arg) const;

  /// @brief Condense params with sigIndex, spill scratchRegs and globals
  /// @param sigIndex Signature type index for the function type
  /// @param isIndirectCall Whether this is an indirect call
  /// @return Iterator to params base
  Stack::iterator prepareCallParamsAndSpillContext(uint32_t const sigIndex, bool const isIndirectCall);

private:
  Compiler &compiler_; ///< Reference to the compiler instance

  bool hasPendingSideEffectInstructions_; ///< Whether there are pending side effect instructions that need to be condensed

  ///
  /// @brief Evaluate a specific COMPARISON DEFERRED ACTION with up to two input operands
  ///
  /// @param instructionPtr Iterator to a StackElement that represents a DEFERREDACTION
  /// @param arg0Ptr First input operand
  /// @param arg1Ptr Second input operand
  /// @return Output BranchCondition (not UNCONDITIONAL)
  BranchCondition evaluateCondition(Stack::iterator const instructionPtr, Stack::iterator const arg0Ptr, Stack::iterator const arg1Ptr) const;

  ///
  /// @brief Evaluate a specific DEFERRED ACTION with up to two input operands
  ///
  /// @param instructionPtr Iterator to a StackElement that represents a DEFERREDACTION
  /// @param arg0Ptr First input operand
  /// @param arg1Ptr Second input operand
  /// @param arg2Ptr Third input operand
  /// @param targetHint Optional target hint that can be used as a scratch register if the underlying MachineType matches
  StackElement evaluateInstruction(Stack::iterator const instructionPtr, Stack::iterator const arg0Ptr, Stack::iterator const arg1Ptr,
                                   Stack::iterator const arg2Ptr, StackElement const *const targetHint) const;

  ///
  /// @brief Condense (resolve) a valent block consisting of one or multiple StackElements into a single semantically
  /// equivalent StackElement that will be put at the bottom of the current stack element. Other StackElements on the
  /// stack will be marked as SKIP for traversal
  ///
  /// @param comparison Whether this is a comparison. If true, this will not leave a result on the stack. The
  /// appropriately set CPU status flags can be considered the result
  /// @param belowIt iterator that represents the next element of the first valent block.
  /// @param enforcedTarget Optional StackElement representing a storage location where the result should be put
  /// @return ConditionResult Base of the valent block and an optional BranchCondition if this was a comparison
  ConditionResult condenseValentBlockCoreBelow(bool const comparison, Stack::iterator const belowIt, StackElement const *const enforcedTarget) const;

  ///
  /// @brief Condense a scratch register in the valent block tree
  /// @param rootNode iterator that represents the first valent block.
  /// @param enforcedTarget Optional StackElement representing a storage location where the result should be put
  void condenseScratchRegBelow(Stack::iterator const rootNode, StackElement const *const enforcedTarget) const;

  ///
  /// @brief Condense side effect instructions in the valent block tree.
  /// @details For better usage of CPU pipelines, the side effect instructions aka. div and memory load need to be scheduled earlier.
  /// @param rootNode iterator that represents the first valent block.
  /// @param enforcedTarget Optional StackElement representing a storage location where the result should be put
  void condenseSideEffectInstructionBelow(Stack::iterator const rootNode, StackElement const *const enforcedTarget) const;

  /// @brief Condense a valent block in the valent block tree unconditionally
  /// @param rootNode iterator that represents the first valent block.
  /// @param enforcedTarget Optional StackElement representing a storage location where the result should be put
  void condenseValentBlockBasic(Stack::iterator const rootNode, StackElement const *const enforcedTarget) const;

  /// @brief Get the first operand of a deferred action, which is the left child in valent block tree
  /// @param instruction Iterator to a deferred action
  /// @return Iterator to the left child of the deferred action
  static Stack::iterator getFirstOperand(Stack::iterator const instruction) VB_NOEXCEPT;

  /// @brief replace a Stack element on condense tree to keep the original parent and sibling
  /// @param originElement The original StackElement to replace
  /// @param newElement The new StackElement to replace the original
  static void replaceInCondenseTree(StackElement &originElement, StackElement const &newElement) VB_NOEXCEPT;

  ///
  /// @brief Check whether a register is a volatile used as callScrRegs
  ///
  /// @param reg Register to check
  /// @return True if the register is call src reg
  static bool isCallScrReg(TReg const reg) VB_NOEXCEPT;

  /// @brief Check whether the current frame is empty
  /// @return True if the current frame is empty
  bool currentFrameEmpty() const VB_NOEXCEPT;

  /// @brief Check whether a stack element is in a register or constant
  /// @param it Iterator to the stack element
  /// @return True if the stack element is in a register or constant
  bool stackElementInRegOrConst(Stack::iterator const it) const VB_NOEXCEPT;

  /// @brief Check whether StackElement is a scratch register and only used once on the stack
  /// @param element Iterator to the stack element
  /// @return True if the stack element is a scratch register and only used once on the stack
  static bool scratchRegOnlyOnceOnStack(Stack::iterator const element) VB_NOEXCEPT;
};

} // namespace vb

#endif /* COMMON_H */

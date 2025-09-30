///
/// @file OPCode.hpp
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
#ifndef OPCODE_HPP
#define OPCODE_HPP

#include <cassert>
#include <cstdint>

#include "BranchCondition.hpp"

namespace vb {

///
/// @brief WebAssembly OPCodes and their basic encoding
/// Currently only supports extend opcode which is less then 255. u16 is enough for unique value for each op code.
///
enum class OPCode : uint16_t {
  SCALAR_EXTEND_OP_CODE = 0xFC,
  SCALAR_EXTEND_OP_CODE_PREFIX = SCALAR_EXTEND_OP_CODE << 8U,
  VECTOR_EXTEND_OP_CODE = 0xFD,

  // CONTROL FLOW OPERATORS
  UNREACHABLE = 0x00,
  NOP = 0x01,
  BLOCK = 0x02,
  LOOP = 0x03,
  IF = 0x04,
  ELSE = 0x05,

  END = 0x0B,
  BR = 0x0C,
  BR_IF = 0x0D,
  BR_TABLE = 0x0E,
  RETURN = 0x0F,

  // CALL OPERATORS
  CALL = 0x10,
  CALL_INDIRECT = 0x11,

  // REFERENCE OPERATORS
  REF_NULL = 0xD0,
  REF_IS_NULL = 0xD1,
  REF_FUNC = 0xD2,

  // PARAMETRIC OPERATORS
  DROP = 0x1A,
  SELECT = 0x1B,
  SELECT_T = 0x1C,

  // VARIABLE ACCESS
  LOCAL_GET = 0x20,
  LOCAL_SET = 0x21,
  LOCAL_TEE = 0x22,
  GLOBAL_GET = 0x23,
  GLOBAL_SET = 0x24,

  // TABLE OPERATORS
  TABLE_GET = 0x25,
  TABLE_SET = 0x26,
  TABLE_INIT = SCALAR_EXTEND_OP_CODE_PREFIX | 12U,
  ELEMENT_DROP = SCALAR_EXTEND_OP_CODE_PREFIX | 13U,
  TABLE_COPY = SCALAR_EXTEND_OP_CODE_PREFIX | 14U,
  TABLE_GROW = SCALAR_EXTEND_OP_CODE_PREFIX | 15U,
  TABLE_SIZE = SCALAR_EXTEND_OP_CODE_PREFIX | 16U,
  TABLE_FILL = SCALAR_EXTEND_OP_CODE_PREFIX | 17U,

  // MEMORY-RELATED OPERATOR
  I32_LOAD = 0x28,
  I64_LOAD = 0x29,
  F32_LOAD = 0x2A,
  F64_LOAD = 0x2B,
  I32_LOAD8_S = 0x2C,
  I32_LOAD8_U = 0x2D,
  I32_LOAD16_S = 0x2E,
  I32_LOAD16_U = 0x2F,
  I64_LOAD8_S = 0x30,
  I64_LOAD8_U = 0x31,
  I64_LOAD16_S = 0x32,
  I64_LOAD16_U = 0x33,
  I64_LOAD32_S = 0x34,
  I64_LOAD32_U = 0x35,
  I32_STORE = 0x36,
  I64_STORE = 0x37,
  F32_STORE = 0x38,
  F64_STORE = 0x39,
  I32_STORE8 = 0x3A,
  I32_STORE16 = 0x3B,
  I64_STORE8 = 0x3C,
  I64_STORE16 = 0x3D,
  I64_STORE32 = 0x3E,
  MEMORY_SIZE = 0x3F,
  MEMORY_GROW = 0x40,
  MEMORY_INIT = SCALAR_EXTEND_OP_CODE_PREFIX | 8U,
  DATA_DROP = SCALAR_EXTEND_OP_CODE_PREFIX | 9U,
  MEMORY_COPY = SCALAR_EXTEND_OP_CODE_PREFIX | 10U,
  MEMORY_FILL = SCALAR_EXTEND_OP_CODE_PREFIX | 11U,

  // CONSTANTS
  I32_CONST = 0x41,
  I64_CONST = 0x42,
  F32_CONST = 0x43,
  F64_CONST = 0x44,

  // COMPARISON OPERATORS + INVERTED CMP OPCODE
  I32_EQZ = 0x45,  // UNREACHABLE
  I32_EQ = 0x46,   // I32_NE
  I32_NE = 0x47,   // I32_EQ
  I32_LT_S = 0x48, // I32_GE_S
  I32_LT_U = 0x49, // I32_GE_U
  I32_GT_S = 0x4A, // I32_LE_S
  I32_GT_U = 0x4B, // I32_LE_U
  I32_LE_S = 0x4C, // I32_GT_S
  I32_LE_U = 0x4D, // I32_GT_U
  I32_GE_S = 0x4E, // I32_LT_S
  I32_GE_U = 0x4F, // I32_LT_U

  I64_EQZ = 0x50,  // UNREACHABLE
  I64_EQ = 0x51,   // I64_NE
  I64_NE = 0x52,   // I64_EQ
  I64_LT_S = 0x53, // I64_GE_S
  I64_LT_U = 0x54, // I64_GE_U
  I64_GT_S = 0x55, // I64_LE_S
  I64_GT_U = 0x56, // I64_LE_U
  I64_LE_S = 0x57, // I64_GT_S
  I64_LE_U = 0x58, // I64_GT_U
  I64_GE_S = 0x59, // I64_LT_S
  I64_GE_U = 0x5A, // I64_LT_U

  F32_EQ = 0x5B, // F32_NE
  F32_NE = 0x5C, // F32_EQ
  F32_LT = 0x5D, // F32_GE
  F32_GT = 0x5E, // F32_LE
  F32_LE = 0x5F, // F32_GT
  F32_GE = 0x60, // F32_LT

  F64_EQ = 0x61, // F64_NE
  F64_NE = 0x62, // F64_EQ
  F64_LT = 0x63, // F64_GE
  F64_GT = 0x64, // F64_LE
  F64_LE = 0x65, // F64_GT
  F64_GE = 0x66, // F64_LT

  // NUMERIC OPERATORS
  I32_CLZ = 0x67,
  I32_CTZ = 0x68,
  I32_POPCNT = 0x69,
  I32_ADD = 0x6A,
  I32_SUB = 0x6B,
  I32_MUL = 0x6C,
  I32_DIV_S = 0x6D,
  I32_DIV_U = 0x6E,
  I32_REM_S = 0x6F,
  I32_REM_U = 0x70,
  I32_AND = 0x71,
  I32_OR = 0x72,
  I32_XOR = 0x73,
  I32_SHL = 0x74,
  I32_SHR_S = 0x75,
  I32_SHR_U = 0x76,
  I32_ROTL = 0x77,
  I32_ROTR = 0x78,

  I64_CLZ = 0x79,
  I64_CTZ = 0x7A,
  I64_POPCNT = 0x7B,
  I64_ADD = 0x7C,
  I64_SUB = 0x7D,
  I64_MUL = 0x7E,
  I64_DIV_S = 0x7F,
  I64_DIV_U = 0x80,
  I64_REM_S = 0x81,
  I64_REM_U = 0x82,
  I64_AND = 0x83,
  I64_OR = 0x84,
  I64_XOR = 0x85,
  I64_SHL = 0x86,
  I64_SHR_S = 0x87,
  I64_SHR_U = 0x88,
  I64_ROTL = 0x89,
  I64_ROTR = 0x8A,

  // movss
  F32_ABS = 0x8B,
  F32_NEG = 0x8C,
  F32_CEIL = 0x8D,
  F32_FLOOR = 0x8E,
  F32_TRUNC = 0x8F,
  F32_NEAREST = 0x90,
  F32_SQRT = 0x91,
  F32_ADD = 0x92,
  F32_SUB = 0x93,
  F32_MUL = 0x94,
  F32_DIV = 0x95,
  F32_MIN = 0x96,
  F32_MAX = 0x97,
  F32_COPYSIGN = 0x98,

  F64_ABS = 0x99,
  F64_NEG = 0x9A,
  F64_CEIL = 0x9B,
  F64_FLOOR = 0x9C,
  F64_TRUNC = 0x9D,
  F64_NEAREST = 0x9E,
  F64_SQRT = 0x9F,
  F64_ADD = 0xA0,
  F64_SUB = 0xA1,
  F64_MUL = 0xA2,
  F64_DIV = 0xA3,
  F64_MIN = 0xA4,
  F64_MAX = 0xA5,
  F64_COPYSIGN = 0xA6,

  // CONVERSIONS
  I32_WRAP_I64 = 0xA7,
  I32_TRUNC_F32_S = 0xA8,
  I32_TRUNC_F32_U = 0xA9,
  I32_TRUNC_F64_S = 0xAA,
  I32_TRUNC_F64_U = 0xAB,

  I64_EXTEND_I32_S = 0xAC,
  I64_EXTEND_I32_U = 0xAD,
  I64_TRUNC_F32_S = 0xAE,
  I64_TRUNC_F32_U = 0xAF,
  I64_TRUNC_F64_S = 0xB0,
  I64_TRUNC_F64_U = 0xB1,

  F32_CONVERT_I32_S = 0xB2,
  F32_CONVERT_I32_U = 0xB3,
  F32_CONVERT_I64_S = 0xB4,
  F32_CONVERT_I64_U = 0xB5,
  F32_DEMOTE_F64 = 0xB6,

  F64_CONVERT_I32_S = 0xB7,
  F64_CONVERT_I32_U = 0xB8,
  F64_CONVERT_I64_S = 0xB9,
  F64_CONVERT_I64_U = 0xBA,
  F64_PROMOTE_F32 = 0xBB,

  // REINTERPRETATIONS
  I32_REINTERPRET_F32 = 0xBC,
  I64_REINTERPRET_F64 = 0xBD,
  F32_REINTERPRET_I32 = 0xBE,
  F64_REINTERPRET_I64 = 0xBF,

  // SIGN EXTENSIONS
  I32_EXTEND8_S = 0xC0,
  I32_EXTEND16_S = 0xC1,
  I64_EXTEND8_S = 0xC2,
  I64_EXTEND16_S = 0xC3,
  I64_EXTEND32_S = 0xC4,

  // SATURATING TRUNCATION
  I32_TRUNC_SAT_F32_S = SCALAR_EXTEND_OP_CODE_PREFIX | 0U,
  I32_TRUNC_SAT_F32_U = SCALAR_EXTEND_OP_CODE_PREFIX | 1U,
  I32_TRUNC_SAT_F64_S = SCALAR_EXTEND_OP_CODE_PREFIX | 2U,
  I32_TRUNC_SAT_F64_U = SCALAR_EXTEND_OP_CODE_PREFIX | 3U,
  I64_TRUNC_SAT_F32_S = SCALAR_EXTEND_OP_CODE_PREFIX | 4U,
  I64_TRUNC_SAT_F32_U = SCALAR_EXTEND_OP_CODE_PREFIX | 5U,
  I64_TRUNC_SAT_F64_S = SCALAR_EXTEND_OP_CODE_PREFIX | 6U,
  I64_TRUNC_SAT_F64_U = SCALAR_EXTEND_OP_CODE_PREFIX | 7U,
};

///
/// @brief Subtraction operator to quickly calculate the difference between two OPCodes
///
/// @param lhs Left hand side input opcode
/// @param rhs Right hand side input opcode
/// @return uint8_t Difference of the encoding of the input opcodes
inline uint8_t operator-(OPCode const lhs, OPCode const rhs) VB_NOEXCEPT {
  return static_cast<uint8_t>(static_cast<uint8_t>(lhs) - static_cast<uint8_t>(rhs));
}

///
/// @brief Get the abstract BranchCondition which is equivalent to a given OPCode
/// NOTE: Undefined behavior if the OPCode is not a comparison
///
/// @param opcode Comparison OPCode
/// @return BC Equivalent abstract BranchCondition
inline BC BCforOPCode(OPCode const opcode) VB_NOEXCEPT {
  assert(opcode >= OPCode::I32_EQZ && opcode <= OPCode::F64_GE && "OPCode is no comparison");
  // coverity[autosar_cpp14_a8_5_2_violation]
  constexpr auto branchCondForOpcode =
      make_array(BC::EQZ, BC::EQ, BC::NE, BC::LT_S, BC::LT_U, BC::GT_S, BC::GT_U, BC::LE_S, BC::LE_U, BC::GE_S, BC::GE_U, BC::EQZ, BC::EQ, BC::NE,
                 BC::LT_S, BC::LT_U, BC::GT_S, BC::GT_U, BC::LE_S, BC::LE_U, BC::GE_S, BC::GE_U, BC::EQ_F, BC::NE_F, BC::LT_F, BC::GT_F, BC::LE_F,
                 BC::GE_F, BC::EQ_F, BC::NE_F, BC::LT_F, BC::GT_F, BC::LE_F, BC::GE_F);
  return branchCondForOpcode[static_cast<uint32_t>(opcode) - static_cast<uint32_t>(OPCode::I32_EQZ)];
}

/// @brief Check if the given OPCode is a division opcode for int32
/// @param opcode Input OPCode
/// @return
inline bool opcodeIsDivInt32(OPCode const opcode) VB_NOEXCEPT {
  uint32_t const opcodeVal{static_cast<uint32_t>(opcode)};
  bool const isDivInt32{(opcodeVal >= static_cast<uint32_t>(OPCode::I32_DIV_S)) && (opcodeVal <= static_cast<uint32_t>(OPCode::I32_REM_U))};
  return isDivInt32;
}

/// @brief Check if the given OPCode is a division opcode for int32 and int64
/// @param opcode Input OPCode
/// @return
inline bool opcodeIsDivInt(OPCode const opcode) VB_NOEXCEPT {
  uint32_t const opcodeVal{static_cast<uint32_t>(opcode)};
  bool const isDivInt{opcodeIsDivInt32(opcode) ||
                      ((opcodeVal >= static_cast<uint32_t>(OPCode::I64_DIV_S)) && (opcodeVal <= static_cast<uint32_t>(OPCode::I64_REM_U)))};
  return isDivInt;
}

/// @brief Check if the given OPCode is float memory load
/// @param opcode Input OPCode
/// @return
inline bool opcodeIsLoadFloat(OPCode const opcode) VB_NOEXCEPT {
  return (opcode == OPCode::F32_LOAD) || (opcode == OPCode::F64_LOAD);
}

/// @brief Check if the given OPCode i32 bit memory load
/// @param opcode Input OPCode
/// @return
inline bool opcodeIsLoad32(OPCode const opcode) VB_NOEXCEPT {
  uint32_t const opcodeVal{static_cast<uint32_t>(opcode)};
  bool const isLoad32{((opcode == OPCode::I32_LOAD) || (opcode == OPCode::F32_LOAD)) ||
                      ((opcodeVal >= static_cast<uint32_t>(OPCode::I32_LOAD8_S)) && (opcodeVal <= static_cast<uint32_t>(OPCode::I32_LOAD16_U)))};
  return isLoad32;
}

} // namespace vb

#endif

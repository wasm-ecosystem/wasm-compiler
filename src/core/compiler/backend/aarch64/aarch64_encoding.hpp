///
/// @file aarch64_encoding.hpp
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
#ifndef AARCH64_ENCODING_HPP
#define AARCH64_ENCODING_HPP

#include <cstdint>

#include "src/core/common/util.hpp"
#include "src/core/compiler/common/BranchCondition.hpp"

namespace vb {
namespace aarch64 {

///
/// @brief Native registers and their encoding that can be placed into the respective fields in an instruction
/// NOTE: REG::NONE will be used to represent an invalid register (or no register at all)
///
// coverity[autosar_cpp14_a7_2_4_violation] autosar required style is more bug prone
enum class REG : uint32_t { // clang-format off
  R0, R1, R2, R3, R4, R5, R6, R7, R8, R9, R10, R11, R12, R13, R14, R15, R16, R17, R18, R19, R20, R21, R22, R23, R24, R25, R26, R27, R28, FP, LR, ZR, SP = ZR,
  F0 = 0b0010'0000, F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12, F13, F14, F15, F16, F17, F18, F19, F20, F21, F22, F23, F24, F25, F26, F27, F28, F29, F30, F31,
  NUMREGS,
  NONE = 0b1000'0000
}; // clang-format on

constexpr uint32_t totalNumRegs{static_cast<uint32_t>(REG::NUMREGS)}; ///< Total number of registers in the enum

namespace RegUtil {

///
/// @brief Checks whether a register is a general purpose register (as opposed to a floating point register)
///
/// @param reg Register to check
/// @return bool Whether the register is a general purpose register
inline bool isGPR(REG const reg) VB_NOEXCEPT {
  return (static_cast<uint32_t>(reg) & 0b10'0000U) == 0U;
}

} // namespace RegUtil

///
/// @brief AArch64 CPU condition codes
///
/// @note
///    Condition   |  Meaning               |  Notes
///       EQ       |  equal                 |  Equal
///       NE       |  not equal             |  Not equal
///       CS       |  carry set             |  Carry set
///       HS       |  high or same          |  Unsigned higher or same
///       CC       |  carry clear           |  Carry clear
///       LO       |  low                   |  Unsigned lower
///       MI       |  minus                 |  Negative
///       PL       |  plus                  |  Positive or zero
///       VS       |  overflow set          |  Signed overflow
///       VC       |  overflow clear        |  No signed overflow
///       HI       |  high                  |  Unsigned higher
///       LS       |  low or same           |  Unsigned lower or same
///       GE       |  greater than or equal |  Signed greater than or equal
///       LT       |  less than             |  Signed less than
///       GT       |  greater than          |  Signed greater than
///       LE       |  less than or equal    |  Signed less than or equal
///       AL       |  always                |  Always executed (unconditional)
///
// coverity[autosar_cpp14_a7_2_4_violation] autosar required style is more bug prone
enum class CC : uint8_t { EQ, NE, CS, HS = CS, CC, LO = CC, MI, PL, VS, VC, HI, LS, GE, LT, GT, LE, AL, NV, NONE = 0xFF };

///
/// @brief Invert the condition code (i.e. return CC::LT from CC:GE)
///
/// @param cc Condition code to invert
/// @return CC Inverted condition code
inline CC negateCC(CC const cc) VB_NOEXCEPT {
  if (cc == CC::NONE) {
    return cc;
  }
  // coverity[autosar_cpp14_a7_2_1_violation]
  return static_cast<CC>(static_cast<uint8_t>(cc) ^ 0b1U);
}

///
/// @brief Find the corresponding CPU condition code to an abstract branch condition
///
/// @param branchCond Input branch condition
/// @return CC Corresponding CPU condition code
inline CC CCforBC(BC const branchCond) VB_NOEXCEPT {
  assert(static_cast<uint8_t>(branchCond) <= static_cast<uint8_t>(BC::UNCONDITIONAL) && "Invalid branch condition");
  // coverity[autosar_cpp14_a8_5_2_violation]
  constexpr auto CCforBC = make_array(CC::NE, CC::EQ, CC::EQ, CC::NE, CC::LT, CC::LO, CC::GT, CC::HI, CC::LE, CC::LS, CC::GE, CC::HS, CC::EQ, CC::NE,
                                      CC::LO, CC::GT, CC::LS, CC::GE, CC::NONE);
  return CCforBC[static_cast<uint8_t>(branchCond)];
}

///
/// @brief Abstract definition for the input argument of an abstract instruction
///
/// This defines the input type (I32, I64, F32, F64) and whether this instruction can handle floating point, general
/// purpose registers or an immediate of a certain encoding. Only the encodings used in selectInstr are defined here and
/// which are thus selected from an array of instructions ArgType::TYPEMASK can be used to extract the underlying input
/// type (I32 etc.) for an ArgType
///
/// The identifier after the underscore denotes the underlying type
/// imm6l and imm6r are used for rotating and shifting left and right, respectively
///
// coverity[autosar_cpp14_a7_2_4_violation] autosar required style is more bug prone
enum class ArgType : uint8_t { // clang-format off
	NONE = 0b00000000,
	I32 = 0b00100000, r32, imm12zxols12_32, imm12bitmask_32, imm6l_32, imm6r_32,
	I64 = 0b01000000, r64, imm12zxols12_64, imm13bitmask_64, imm6l_64, imm6r_64,
	F32 = 0b10000000, r32f,
	F64 = 0b01100000, r64f,
	TYPEMASK = 0b11100000
}; // clang-format on

///
/// @brief Basic template for AArch64 OPCodes
///
using OPCodeTemplate = uint32_t;

///
/// @brief Complete description of an AArch64 instruction
///
/// This includes an opcode template, the destination and source types and whether the sources are commutative
/// NOTE: For readonly instructions like CMP, dstType is ArgType::NONE, for instructions only taking a single input,
/// src1Type is ArgType::NONE Commutation of source inputs is designed in such a way that an instruction is considered
/// source-commutative if the data in the destination after execution is the same if the source inputs are swapped
///
struct AbstrInstr final {
  OPCodeTemplate opcode;    ///< Basic opcode template
  ArgType dstType;          ///< Destination type
  ArgType src0Type;         ///< First source type
  ArgType src1Type;         ///< Second source type
  bool src_0_1_commutative; ///< Whether first and second source are commutative
};

/// @brief CLZ Wd, Wn: Count Leading Zeros
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr CLZ_wD_wN{0x5AC01000U, ArgType::r32, ArgType::r32, ArgType::NONE, false};
/// @brief CLZ Xd, Xn: Count Leading Zeros
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr CLZ_xD_xN{0xDAC01000U, ArgType::r64, ArgType::r64, ArgType::NONE, false};

/// @brief RBIT Wd, Wn: Reverse Bits
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr RBIT_wD_wN{0x5AC00000U, ArgType::r32, ArgType::r32, ArgType::NONE, false};
/// @brief RBIT Xd, Xn: Reverse Bits
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr RBIT_xD_xN{0xDAC00000U, ArgType::r64, ArgType::r64, ArgType::NONE, false};

/// @brief ADD Wd, Wn, Wm{, shift amount}: Adds a register value and an optionally-shifted (0-31) register value
constexpr AbstrInstr ADD_wD_wN_wMolsImm6{0x0B000000U, ArgType::r32, ArgType::r32, ArgType::r32, true};
/// @brief ADD Xd, Xn, Xm{, shift amount}: Adds a register value and an optionally-shifted (0-63) register value
constexpr AbstrInstr ADD_xD_xN_xMolsImm6{0x8B000000U, ArgType::r64, ArgType::r64, ArgType::r64, true};
/// @brief ADD Wd|WSP, Wn|WSP, imm{, shift}: Adds a register value and an optionally-shifted immediate value (0-4095,
/// optional left shift by 12) from a register value
constexpr AbstrInstr ADD_wD_wN_imm12zxols12{0x11000000U, ArgType::r32, ArgType::r32, ArgType::imm12zxols12_32, true};
/// @brief ADD Xd|SP, Xn|SP, imm{, shift}: Adds a register value and an optionally-shifted immediate value (0-4095,
/// optional left shift by 12) from a register value
constexpr AbstrInstr ADD_xD_xN_imm12zxols12{0x91000000U, ArgType::r64, ArgType::r64, ArgType::imm12zxols12_64, true};

/// @brief ADDS Wd, Wn, Wm: Adds a register value and sets flags
constexpr AbstrInstr ADDS_wD_wN_wM{0x2B000000U, ArgType::r32, ArgType::r32, ArgType::r32, true};
/// @brief ADDS Wd, Wn|WSP, imm{, shift}: Adds a register value and an optionally-shifted immediate value (0-4095,
/// optional left shift by 12) and sets flags
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ADDS_wD_wN_imm12zxols12{0x31000000U, ArgType::r32, ArgType::r32, ArgType::imm12zxols12_32, true};

/// @brief SUB Wd, Wn, Wm{, shift amount}: Subtracts an optionally-shifted (0-31) register value from a register value
constexpr AbstrInstr SUB_wD_wN_wMolsImm6{0x4B000000U, ArgType::r32, ArgType::r32, ArgType::r32, false};
/// @brief SUB Xd, Xn, Xm{, shift amount} Subtracts an optionally-shifted (0-63) register value from a register value
constexpr AbstrInstr SUB_xD_xN_xMolsImm6{0xCB000000U, ArgType::r64, ArgType::r64, ArgType::r64, false};
/// @brief SUB Wd|WSP, Wn|WSP, imm{, shift}: Subtracts an optionally-shifted immediate value (0-4095, optional left
/// shift by 12 bits) from a register value
constexpr AbstrInstr SUB_wD_wN_imm12zxols12{0x51000000U, ArgType::r32, ArgType::r32, ArgType::imm12zxols12_32, false};
/// @brief SUB Xd|SP, Xn|SP, imm{, shift}: Subtracts an optionally-shifted immediate value (0-4095, optional left shift
/// by 12 bits) from a register value
constexpr AbstrInstr SUB_xD_xN_imm12zxols12{0xD1000000U, ArgType::r64, ArgType::r64, ArgType::imm12zxols12_64, false};

/// @brief SUB Xd|SP, SP, Xm: Subtracts a register from the stack pointer
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate SUB_xD_SP_xM_t{0xCB2063E0U};

/// @brief SUBS Wd, Wn, Wm: Subtracts a register value and sets flags
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SUBS_wD_wN_wM{0x6B000000U, ArgType::r32, ArgType::r32, ArgType::r32, true};
/// @brief SUBS Wd, Wn|WSP, imm{, shift} Subtracts an optionally-shifted immediate value (0-4095, optional left shift by
/// 12 bits) from a register value and sets flags
constexpr AbstrInstr SUBS_wD_wN_imm12zxols12{0x71000000U, ArgType::r32, ArgType::r32, ArgType::imm12zxols12_32, false};
/// @brief SUBS Xd, Xn|SP, imm{, shift} Subtracts an optionally-shifted immediate value (0-4095, optional left shift by
/// 12 bits) from a register value and sets flags
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SUBS_xD_xN_imm12zxols12{0xF1000000U, ArgType::r64, ArgType::r64, ArgType::imm12zxols12_64, false};

/// @brief MUL Wd, Wn, Wm: Multiplies two register values
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr MUL_wD_wN_wM{0x1B007C00U, ArgType::r32, ArgType::r32, ArgType::r32, true};
/// @brief MUL Xd, Xn, Xm: Multiplies two register values
constexpr AbstrInstr MUL_xD_xN_xM{0x9B007C00U, ArgType::r64, ArgType::r64, ArgType::r64, true};
/// @brief SDIV Wd, Wn, Wm: Divides a signed integer register value by another signed integer register value
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SDIV_wD_wN_wM{0x1AC00C00U, ArgType::r32, ArgType::r32, ArgType::r32, false};
/// @brief SDIV Xd, Xn, Xm: Divides a signed integer register value by another signed integer register value
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SDIV_xD_xN_xM{0x9AC00C00U, ArgType::r64, ArgType::r64, ArgType::r64, false};
/// @brief UDIV Wd, Wn, Wm: Divides an unsigned integer register value by another unsigned integer register value
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr UDIV_wD_wN_wM{0x1AC00800U, ArgType::r32, ArgType::r32, ArgType::r32, false};
/// @brief UDIV Xd, Xn, Xm: Divides an unsigned integer register value by another unsigned integer register value
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr UDIV_xD_xN_xM{0x9AC00800U, ArgType::r64, ArgType::r64, ArgType::r64, false};

/// @brief AND Wd, Wn, Wm: Bitwise AND of a register value and another register value
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr AND_wD_wN_wM{0x0A000000U, ArgType::r32, ArgType::r32, ArgType::r32, true};
/// @brief AND Xd, Xn, Xm: Bitwise AND of a register value and another register value
constexpr AbstrInstr AND_xD_xN_xM{0x8A000000U, ArgType::r64, ArgType::r64, ArgType::r64, true};
/// @brief AND Wd|WSP, Wn, imm: Bitwise AND of a register value and an immediate value (12-bit bitmask)
/// NOTE: This instruction template is not valid as-is since the bitmask cannot encode a zero
constexpr AbstrInstr AND_wD_wN_imm12bitmask{0x12000000U, ArgType::r32, ArgType::r32, ArgType::imm12bitmask_32, true};
/// @brief AND Xd|SP, Xn, imm: Bitwise AND of a register value and an immediate value (13-bit bitmask)
/// NOTE: This instruction template is not valid as-is since the bitmask cannot encode a zero
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr AND_xD_xN_imm13bitmask{0x92000000U, ArgType::r64, ArgType::r64, ArgType::imm13bitmask_64, true};

/// @brief ORR Wd, Wn, Wm: Bitwise (inclusive) OR of a register value and another register value
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ORR_wD_wN_wM{0x2A000000U, ArgType::r32, ArgType::r32, ArgType::r32, true};
/// @brief ORR Xd, Xn, Xm: Bitwise (inclusive) OR of a register value and another register value
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ORR_xD_xN_xM{0xAA000000U, ArgType::r64, ArgType::r64, ArgType::r64, true};
/// @brief ORR Wd|WSP, Wn, imm: Bitwise (inclusive) OR of a register value and an immediate value (12-bit bitmask)
/// NOTE: This instruction template is not valid as-is since the bitmask cannot encode a zero
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ORR_wD_wN_imm12bitmask{0x32000000U, ArgType::r32, ArgType::r32, ArgType::imm12bitmask_32, true};
/// @brief ORR Xd|SP, Xn, imm: Bitwise (inclusive) OR of a register value and an immediate value (13-bit bitmask)
/// NOTE: This instruction template is not valid as-is since the bitmask cannot encode a zero
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ORR_xD_xN_imm13bitmask{0xB2000000U, ArgType::r64, ArgType::r64, ArgType::imm13bitmask_64, true};

/// @brief EOR Wd, Wn, Wm: Bitwise exclusive OR of a register value and another register value
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr EOR_wD_wN_wM{0x4A000000U, ArgType::r32, ArgType::r32, ArgType::r32, true};
/// @brief EOR Xd, Xn, Xm: Bitwise exclusive OR of a register value and another register value
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr EOR_xD_xN_xM{0xCA000000U, ArgType::r64, ArgType::r64, ArgType::r64, true};
/// @brief EOR Wd|WSP, Wn, imm: Bitwise exclusive OR of a register value and an immediate value (12-bit bitmask)
/// NOTE: This instruction template is not valid as-is since the bitmask cannot encode a zero
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr EOR_wD_wN_imm12bitmask{0x52000000U, ArgType::r32, ArgType::r32, ArgType::imm12bitmask_32, true};
/// @brief EOR Xd|SP, Xn, imm: Bitwise exclusive OR of a register value and an immediate value (13-bit bitmask)
/// NOTE: This instruction template is not valid as-is since the bitmask cannot encode a zero
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr EOR_xD_xN_imm13bitmask{0xD2000000U, ArgType::r64, ArgType::r64, ArgType::imm13bitmask_64, true};

/// @brief LSL Wd, Wn, Wm: (Logically) Shifts a register value left by a variable number of bits, shifting in zeros
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr LSL_wD_wN_wM{0x1AC02000U, ArgType::r32, ArgType::r32, ArgType::r32, false};
/// @brief LSL Xd, Xn, Xm: (Logically) Shifts a register value left by a variable number of bits, shifting in zeros
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr LSL_xD_xN_xM{0x9AC02000U, ArgType::r64, ArgType::r64, ArgType::r64, false};
/// @brief LSL Wd, Wn, shift: (Logically) Shifts a register value left by an immediate number of bits, shifting in zeros
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr LSL_wD_wN_imm6x{0x53000000U, ArgType::r32, ArgType::r32, ArgType::imm6l_32, false};
/// @brief LSL Xd, Xn, shift: (Logically) Shifts a register value left by an immediate number of bits, shifting in zeros
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr LSL_xD_xN_imm6x{0xD3400000U, ArgType::r64, ArgType::r64, ArgType::imm6l_64, false};

/// @brief ASR Wd, Wn, Wm: (Arithmetically) Shifts a register value right by a variable number of bits, shifting in
/// copies of its sign bit
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ASR_wD_wN_wM{0x1AC02800U, ArgType::r32, ArgType::r32, ArgType::r32, false};
/// @brief ASR Xd, Xn, Xm: (Arithmetically) Shifts a register value right by a variable number of bits, shifting in
/// copies of its sign bit
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ASR_xD_xN_xM{0x9AC02800U, ArgType::r64, ArgType::r64, ArgType::r64, false};
/// @brief ASR Wd, Wn, shift: (Arithmetically) Shifts a register value right by an immediate number of bits, shifting in
/// copies of the sign bit in the upper bits
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ASR_wD_wN_imm6x{0x13000000U, ArgType::r32, ArgType::r32, ArgType::imm6r_32, false};
/// @brief ASR Xd, Xn, shift: (Arithmetically) Shifts a register value right by an immediate number of bits, shifting in
/// copies of the sign bit in the upper bits
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ASR_xD_xN_imm6x{0x93400000U, ArgType::r64, ArgType::r64, ArgType::imm6r_64, false};

/// @brief LSR Wd, Wn, Wm: (Logically) Shifts a register value right by a variable number of bits, shifting in zeros
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr LSR_wD_wN_wM{0x1AC02400U, ArgType::r32, ArgType::r32, ArgType::r32, false};
/// @brief LSR Xd, Xn, Xm: (Logically) Shifts a register value right by a variable number of bits, shifting in zeros
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr LSR_xD_xN_xM{0x9AC02400U, ArgType::r64, ArgType::r64, ArgType::r64, false};
/// @brief LSR Wd, Wn, shift: (Logically) Shifts a register value right by an immediate number of bits, shifting in
/// zeros
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr LSR_wD_wN_imm6x{0x53000000U, ArgType::r32, ArgType::r32, ArgType::imm6r_32, false};
/// @brief LSR Xd, Xn, shift: (Logically) Shifts a register value right by an immediate number of bits, shifting in
/// zeros
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr LSR_xD_xN_imm6x{0xD3400000U, ArgType::r64, ArgType::r64, ArgType::imm6r_64, false};

/// @brief ROR Wd, Wn, Wm: Provides the value of the contents of a register rotated by a variable number of bits. The
/// bits that are rotated off the right end are inserted into the vacated bit positions on the left.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ROR_wD_wN_wM{0x1AC02C00U, ArgType::r32, ArgType::r32, ArgType::r32, false};
/// @brief ROR Xd, Xn, Xm: Provides the value of the contents of a register rotated by a variable number of bits. The
/// bits that are rotated off the right end are inserted into the vacated bit positions on the left.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ROR_xD_xN_xM{0x9AC02C00U, ArgType::r64, ArgType::r64, ArgType::r64, false};

/// @brief EXTR Wd, Wn, Wm, lsb: Extracts a register from a pair of registers
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate EXTR_wD_wN_wM_imm6_t{0x13800000U};
/// @brief EXTR Xd, Xn, Xm, lsb: Extracts a register from a pair of registers
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate EXTR_xD_xN_xM_imm6_t{0x93C00000U};

/// @brief FABS Sd, Sn: Calculates the absolute value in the source register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FABS_sD_sN{0x1E20C000U, ArgType::r32f, ArgType::r32f, ArgType::NONE, false};
/// @brief FABS Dd, Dn: Calculates the absolute value in the source register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FABS_dD_dN{0x1E60C000U, ArgType::r64f, ArgType::r64f, ArgType::NONE, false};
/// @brief FNEG Sd, Sn: Negates the value in the source register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FNEG_sD_sN{0x1E214000U, ArgType::r32f, ArgType::r32f, ArgType::NONE, false};
/// @brief FNEG Dd, Sn: Negates the value in the source register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FNEG_dD_dN{0x1E614000U, ArgType::r64f, ArgType::r64f, ArgType::NONE, false};
/// @brief FRINTP Sd, Sn: Rounds a floating-point value in the source register to an integral floating-point value of
/// the same size using the "Round towards Plus infinity" rounding mode
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FRINTP_sD_sN{0x1E24C000U, ArgType::r32f, ArgType::r32f, ArgType::NONE, false};
/// @brief FRINTP Dd, Dn: Rounds a floating-point value in the source register to an integral floating-point value of
/// the same size using the "Round towards Plus infinity" rounding mode
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FRINTP_dD_dN{0x1E64C000U, ArgType::r64f, ArgType::r64f, ArgType::NONE, false};
/// @brief FRINTM Sd, Sn: Rounds a floating-point value in the source register to an integral floating-point value of
/// the same size using the "Round towards Minus infinity" rounding mode
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FRINTM_sD_sN{0x1E254000U, ArgType::r32f, ArgType::r32f, ArgType::NONE, false};
/// @brief FRINTM Dd, Dn: Rounds a floating-point value in the source register to an integral floating-point value of
/// the same size using the "Round towards Minus infinity" rounding mode
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FRINTM_dD_dN{0x1E654000U, ArgType::r64f, ArgType::r64f, ArgType::NONE, false};
/// @brief FRINTZ Sd, Sn: Rounds a floating-point value in the source register to an integral floating-point value of
/// the same size using the "Round towards Zero" rounding mode
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FRINTZ_sD_sN{0x1E25C000U, ArgType::r32f, ArgType::r32f, ArgType::NONE, false};
/// @brief FRINTZ Dd, Dn: Rounds a floating-point value in the source register to an integral floating-point value of
/// the same size using the "Round towards Zero" rounding mode
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FRINTZ_dD_dN{0x1E65C000U, ArgType::r64f, ArgType::r64f, ArgType::NONE, false};
/// @brief FRINTN Sd, Sn: Rounds a floating-point value in the source register to an integral floating-point value of
/// the same size using the "Round towards Nearest" rounding mode
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FRINTN_sD_sN{0x1E244000U, ArgType::r32f, ArgType::r32f, ArgType::NONE, false};
/// @brief FRINTN Dd, Dn: Rounds a floating-point value in the source register to an integral floating-point value of
/// the same size using the "Round towards Nearest" rounding mode
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FRINTN_dD_dN{0x1E644000U, ArgType::r64f, ArgType::r64f, ArgType::NONE, false};
/// @brief FSQRT Sd, Sn: Calculates the square root of the value in the source register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FSQRT_sD_sN{0x1E21C000U, ArgType::r32f, ArgType::r32f, ArgType::NONE, false};
/// @brief FSQRT Dd, Dn: Calculates the square root of the value in the source register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FSQRT_dD_dN{0x1E61C000U, ArgType::r64f, ArgType::r64f, ArgType::NONE, false};
/// @brief FADD Sd, Sn, Sm: Adds the floating-point values of the two source registers
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FADD_sD_sN_sM{0x1E202800U, ArgType::r32f, ArgType::r32f, ArgType::r32f, true};
/// @brief FADD Dd, Dn, Dm: Adds the floating-point values of the two source registers
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FADD_dD_dN_dM{0x1E602800U, ArgType::r64f, ArgType::r64f, ArgType::r64f, true};
/// @brief FSUB Sd, Sn, Sm: Subtracts the floating-point value of the second source register from the floating-point
/// value of the first source register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FSUB_sD_sN_sM{0x1E203800U, ArgType::r32f, ArgType::r32f, ArgType::r32f, false};
/// @brief FSUB Dd, Dn, Dm: Subtracts the floating-point value of the second source register from the floating-point
/// value of the first source register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FSUB_dD_dN_dM{0x1E603800U, ArgType::r64f, ArgType::r64f, ArgType::r64f, false};
/// @brief FMUL Sd, Sn, Sm: Multiplies the floating-point values of the two source registers
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FMUL_sD_sN_sM{0x1E200800U, ArgType::r32f, ArgType::r32f, ArgType::r32f, true};
/// @brief FMUL Dd, Dn, Dm: Multiplies the floating-point values of the two source registers
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FMUL_dD_dN_dM{0x1E600800U, ArgType::r64f, ArgType::r64f, ArgType::r64f, true};
/// @brief FDIV Sd, Sn, Sm: Divides the floating-point value of the first source register by the floating-point value of
/// the second source register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FDIV_sD_sN_sM{0x1E201800U, ArgType::r32f, ArgType::r32f, ArgType::r32f, false};
/// @brief FDIV Dd, Dn, Dm: Divides the floating-point value of the first source register by the floating-point value of
/// the second source register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FDIV_dD_dN_dM{0x1E601800U, ArgType::r64f, ArgType::r64f, ArgType::r64f, false};
/// @brief FMIN Sd, Sn, Sm: Compares the two source registers and writes the smaller of the two floating-point values to
/// the destination register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FMIN_sD_sN_sM{0x1E205800U, ArgType::r32f, ArgType::r32f, ArgType::r32f, true};
/// @brief FMIN Dd, Dn, Dm: Compares the two source registers and writes the smaller of the two floating-point values to
/// the destination register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FMIN_dD_dN_dM{0x1E605800U, ArgType::r64f, ArgType::r64f, ArgType::r64f, true};
/// @brief FMAX Sd, Sn, Sm: Compares the two source registers and writes the larger of the two floating-point values to
/// the destination register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FMAX_sD_sN_sM{0x1E204800U, ArgType::r32f, ArgType::r32f, ArgType::r32f, true};
/// @brief FMAX Dd, Dn, Dm: Compares the two source registers and writes the larger of the two floating-point values to
/// the destination register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FMAX_dD_dN_dM{0x1E604800U, ArgType::r64f, ArgType::r64f, ArgType::r64f, true};

/// @brief FCVTZS Wd, Sn: Converts the single-precision floating-point value in the source register to a 32-bit signed
/// integer using the "Round towards Zero" rounding mode
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FCVTZS_wD_sN{0x1E380000U, ArgType::r32, ArgType::r32f, ArgType::NONE, false};
/// @brief FCVTZS Xd, Sn: Converts the single-precision floating-point value in the source register to a 64-bit signed
/// integer using the "Round towards Zero" rounding mode
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FCVTZS_xD_sN{0x9E380000U, ArgType::r64, ArgType::r32f, ArgType::NONE, false};
/// @brief FCVTZS Wd, Dn: Converts the double-precision floating-point value in the source register to a 32-bit signed
/// integer using the "Round towards Zero" rounding mode
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FCVTZS_wD_dN{0x1E780000U, ArgType::r32, ArgType::r64f, ArgType::NONE, false};
/// @brief FCVTZS Xd, Dn: Converts the double-precision floating-point value in the source register to a 64-bit signed
/// integer using the "Round towards Zero" rounding mode
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FCVTZS_xD_dN{0x9E780000U, ArgType::r64, ArgType::r64f, ArgType::NONE, false};

/// @brief FCVTZU Wd, Sn: Converts the single-precision floating-point value in the source register to a 32-bit unsigned
/// integer using the "Round towards Zero" rounding mode
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FCVTZU_wD_sN{0x1E390000U, ArgType::r32, ArgType::r32f, ArgType::NONE, false};
/// @brief FCVTZU Xd, Sn: Converts the single-precision floating-point value in the source register to a 64-bit unsigned
/// integer using the "Round towards Zero" rounding mode
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FCVTZU_xD_sN{0x9E390000U, ArgType::r64, ArgType::r32f, ArgType::NONE, false};
/// @brief FCVTZU Wd, Dn: Converts the double-precision floating-point value in the source register to a 32-bit unsigned
/// integer using the "Round towards Zero" rounding mode
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FCVTZU_wD_dN{0x1E790000U, ArgType::r32, ArgType::r64f, ArgType::NONE, false};
/// @brief FCVTZU Xd, Dn: Converts the double-precision floating-point value in the source register to a 64-bit unsigned
/// integer using the "Round towards Zero" rounding mode
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FCVTZU_xD_dN{0x9E790000U, ArgType::r64, ArgType::r64f, ArgType::NONE, false};

/// @brief SXTW Xd, Wn: Sign-extends a byte to a word
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SXTB_wD_wN{0x13001C00U, ArgType::r32, ArgType::r32, ArgType::NONE, false};
/// @brief SXTW Xd, Wn: Sign-extends a halfword to a word
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SXTH_wD_wN{0x13003C00U, ArgType::r32, ArgType::r32, ArgType::NONE, false};
/// @brief SXTW Xd, Wn: Sign-extends a byte to a doubleword
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SXTB_xD_xN{0x93401C00U, ArgType::r64, ArgType::r64, ArgType::NONE, false};
/// @brief SXTW Xd, Wn: Sign-extends a halfword to a doubleword
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SXTH_xD_xN{0x93403C00U, ArgType::r64, ArgType::r64, ArgType::NONE, false};
/// @brief SXTW Xd, Wn: Sign-extends a word to a doubleword
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SXTW_xD_xN{0x93407C00U, ArgType::r64, ArgType::r64, ArgType::NONE, false};
/// @brief SXTW Xd, Wn: Sign-extends a word to a doubleword
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SXTW_xD_wN{0x93407C00U, ArgType::r64, ArgType::r32, ArgType::NONE, false};
/// @brief UXTW Xd, Wn: Zero-extends a word to a doubleword
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr UXTW_xD_wN{0xD3407C00U, ArgType::r64, ArgType::r32, ArgType::NONE, false};

/// @brief SCVTF Sd, Wn: Converts the signed integer value in the general-purpose source register to a floating-point
/// value using the rounding mode that is specified by the FPCR
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SCVTF_sD_wN{0x1E220000U, ArgType::r32f, ArgType::r32, ArgType::NONE, false};
/// @brief SCVTF Dd, Wn: Converts the signed integer value in the general-purpose source register to a floating-point
/// value using the rounding mode that is specified by the FPCR
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SCVTF_dD_wN{0x1E620000U, ArgType::r64f, ArgType::r32, ArgType::NONE, false};
/// @brief SCVTF Sd, Xn: Converts the signed integer value in the general-purpose source register to a floating-point
/// value using the rounding mode that is specified by the FPCR
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SCVTF_sD_xN{0x9E220000U, ArgType::r32f, ArgType::r64, ArgType::NONE, false};
/// @brief SCVTF Dd, Xn: Converts the signed integer value in the general-purpose source register to a floating-point
/// value using the rounding mode that is specified by the FPCR
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SCVTF_dD_xN{0x9E620000U, ArgType::r64f, ArgType::r64, ArgType::NONE, false};

/// @brief UCVTF Sd, Wn: Converts the unsigned integer value in the general-purpose source register to a floating-point
/// value using the rounding mode that is specified by the FPCR
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr UCVTF_sD_wN{0x1E230000U, ArgType::r32f, ArgType::r32, ArgType::NONE, false};
/// @brief UCVTF Dd, Wn: Converts the unsigned integer value in the general-purpose source register to a floating-point
/// value using the rounding mode that is specified by the FPCR
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr UCVTF_dD_wN{0x1E630000U, ArgType::r64f, ArgType::r32, ArgType::NONE, false};
/// @brief UCVTF Sd, Xn: Converts the unsigned integer value in the general-purpose source register to a floating-point
/// value using the rounding mode that is specified by the FPCR
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr UCVTF_sD_xN{0x9E230000U, ArgType::r32f, ArgType::r64, ArgType::NONE, false};
/// @brief UCVTF Dd, Xn: Converts the unsigned integer value in the general-purpose source register to a floating-point
/// value using the rounding mode that is specified by the FPCR
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr UCVTF_dD_xN{0x9E630000U, ArgType::r64f, ArgType::r64, ArgType::NONE, false};

/// @brief FCVT Sd, Dn: Converts the double-precision floating-point value in the source register to single-precision
/// using the rounding mode that is specified by the FPCR
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FCVT_sD_dN{0x1E624000U, ArgType::r32f, ArgType::r64f, ArgType::NONE, false};
/// @brief FCVT Dd, Sn: Converts the single-precision floating-point value in the source register to double-precision
/// using the rounding mode that is specified by the FPCR
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FCVT_dD_sN{0x1E22C000U, ArgType::r64f, ArgType::r32f, ArgType::NONE, false};

/// @brief FMOV Wd, Sn: Transfers the contents of the single-precision floating-point register to a 32-bit
/// general-purpose register
constexpr AbstrInstr FMOV_wD_sN{0x1E260000U, ArgType::r32, ArgType::r32f, ArgType::NONE, false};
/// @brief FMOV Wd, Sn: Transfers the contents of the double-precision floating-point register to a 64-bit
/// general-purpose register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FMOV_xD_dN{0x9E660000U, ArgType::r64, ArgType::r64f, ArgType::NONE, false};

/// @brief FMOV Sd, Wn: Transfers the contents of the 32-bit general-purpose register to a single-precision
/// floating-point register
constexpr AbstrInstr FMOV_sD_wN{0x1E270000U, ArgType::r32f, ArgType::r32, ArgType::NONE, false};
/// @brief FMOV Sd, Wn: Transfers the contents of the 64-bit general-purpose register to a double-precision
/// floating-point register
constexpr AbstrInstr FMOV_dD_xN{0x9E670000U, ArgType::r64f, ArgType::r64, ArgType::NONE, false};

/// @brief FMOV Sd, Sn: Copies the single-precision floating-point value in the source register to the single-precision
/// floating-point destination register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FMOV_sD_sN{0x1E204000U, ArgType::r32f, ArgType::r32f, ArgType::NONE, false};
/// @brief FMOV Dd, Dn: Copies the double-precision floating-point value in the source register to the double-precision
/// floating-point destination register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FMOV_dD_dN{0x1E604000U, ArgType::r64f, ArgType::r64f, ArgType::NONE, false};

/// @brief FCMP Sn, Sm: Compares the two single-precision floating-point source register values and updates the
/// condition flags
constexpr AbstrInstr FCMP_sN_sM{0x1E202000U, ArgType::NONE, ArgType::r32f, ArgType::r32f, false};
/// @brief FCMP Dn, Dm: Compares the two double-precision floating-point source register values and updates the
/// condition flags
constexpr AbstrInstr FCMP_dN_dM{0x1E602000U, ArgType::NONE, ArgType::r64f, ArgType::r64f, false};

/// @brief CMP Wn, Wm: Subtracts a register value from a register value and updates the condition flags
constexpr AbstrInstr CMP_wN_wM{0x6B00001FU, ArgType::NONE, ArgType::r32, ArgType::r32, false};
/// @brief CMP Xn, Xm: Subtracts a register value from a register value and updates the condition flags
constexpr AbstrInstr CMP_xN_xM{0xEB00001FU, ArgType::NONE, ArgType::r64, ArgType::r64, false};
/// @brief CMP Wn|WSP, imm{, shift}: Subtracts an optionally-shifted (by 12 bits) immediate value from a register value
/// and updates the condition flags
constexpr AbstrInstr CMP_wN_imm12zxols12{0x7100001FU, ArgType::NONE, ArgType::r32, ArgType::imm12zxols12_32, false};
/// @brief CMP Xn|SP, imm{, shift}: Subtracts an optionally-shifted (by 12 bits) immediate value from a register value
/// and updates the condition flags
constexpr AbstrInstr CMP_xN_imm12zxols12{0xF100001FU, ArgType::NONE, ArgType::r64, ArgType::imm12zxols12_64, false};

/// @brief CMP SP, Xm: Subtracts a register value from the stack pointer and updates the condition flags
constexpr OPCodeTemplate CMP_SP_xM_t{0xEB2063FFU};

/// @brief TST Wn|WSP, imm{, shift}: Bitwise AND an optionally-shifted (by 12 bits) immediate value from a register
/// value and updates the condition flags
constexpr AbstrInstr TST_wN_imm12zxols12{0x7200001FU, ArgType::NONE, ArgType::r32, ArgType::imm12zxols12_32, false};

/// @brief MSUB Wd, Wn, Wm, Wa: Multiplies two register values and subtracts the product from a third register value
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MSUB_wD_wN_wM_wA_t{0x1B008000U};
/// @brief MSUB Wd, Wn, Wm, Wa: Multiplies two register values and subtracts the product from a third register value
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MSUB_xD_xN_xM_xA_t{0x9B008000U};

/// @brief MOVZ Wd, imm{, LSL shift}: Moves an optionally-shifted (0, 16, 32 or 48) 16-bit immediate value to a
/// register, setting other bits to zero
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOVZ_wD_imm16ols_t{0x52800000U};
/// @brief MOVZ Xd, imm{, LSL shift}: Moves an optionally-shifted (0, 16, 32 or 48) 16-bit immediate value to a
/// register, setting other bits to zero
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOVZ_xD_imm16ols_t{0xD2800000U};
/// @brief MOVN Wd, imm{, LSL shift}: Moves the inverse of an optionally-shifted (0, 16, 32 or 48) 16-bit immediate
/// value to a register, setting other bits to one
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOVN_wD_imm16ols_t{0x12800000U};
/// @brief MOVN Xd, imm{, LSL shift}: Moves the inverse of an optionally-shifted (0, 16, 32 or 48) 16-bit immediate
/// value to a register, setting other bits to one
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOVN_xD_imm16ols_t{0x92800000U};
/// @brief MOVK Wd, imm{, LSL shift}: Moves an optionally-shifted (0, 16, 32 or 48) 16-bit immediate value to a
/// register, keeping other bits unchanged
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOVK_wD_imm16ols_t{0x72800000U};
/// @brief MOVK Xd, imm{, LSL shift}: Moves an optionally-shifted (0, 16, 32 or 48) 16-bit immediate value to a
/// register, keeping other bits unchanged
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOVK_xD_imm16ols_t{0xF2800000U};
/// @brief MOV Wd|WSP, imm: Writes a bitmask immediate value (12 bits) to a register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOV_wD_imm12bitmask_t{0x320003E0U};
/// @brief MOV Xd|WSP, imm: Writes a bitmask immediate value (13 bits) to a register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOV_xD_imm13bitmask_t{0xB20003E0U};

/// @brief FMOV Sd, imm: Copies a floating-point immediate constant (encoded as 8-bit modified immediate constant) to
/// the single-precision floating-point destination register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate FMOV_sD_imm8mod_t{0x1E201000U};
/// @brief FMOV Dd, imm: Copies a floating-point immediate constant (encoded as 8-bit modified immediate constant) to
/// the double-precision floating-point destination register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate FMOV_dD_imm8mod_t{0x1E601000U};

/// @brief B label: Branch unconditionally to a label at a PC-relative offset (in the range +/-128MB, encoded as "imm26"
/// times 4)
constexpr OPCodeTemplate B_imm26sxls2_t{0x14000000U};
/// @brief B.cond label: Branch conditionally to a label at a PC-relative offset (in the range +/-1MB, is encoded as
/// "imm19" times 4) NOTE: Use setCond(true, ...)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate Bcondl_imm19sxls2_t{0x54000000U};
/// @brief BR Xn: Branches unconditionally to an address in a register
constexpr OPCodeTemplate BR_xN_t{0xD61F0000U};
/// @brief BL label: Branches to a PC-relative offset (in the range +/-128MB, encoded as "imm26" times 4), setting the
/// register X30/LR to PC+4
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate BL_imm26sxls2_t{0x94000000U};
/// @brief BLR Xn: Calls a subroutine at an address in a register, setting register X30/LR to PC+4
constexpr OPCodeTemplate BLR_xN_t{0xD63F0000U};
/// @brief RET {Xn}: Branches unconditionally to an address in a register
constexpr OPCodeTemplate RET_xN_t{0xD65F0000U};

/// @brief CBZ Wt, label: Compare and branch if register is zero
constexpr OPCodeTemplate CBZ_wT_imm19sxls2_t{0x34000000U};
/// @brief CBZ Xt, label: Compare and branch if register is zero
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate CBZ_xT_imm19sxls2_t{0xB4000000U};
/// @brief CBNZ Wt, label: Compare and branch if register is not zero
constexpr OPCodeTemplate CBNZ_wT_imm19sxls2_t{0x35000000U};
/// @brief CBNZ Xt, label: Compare and branch if register is not zero
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate CBNZ_xT_imm19sxls2_t{0xB5000000U};

/// @brief CSEL Wd, Wn, Wm, cond: Returns, in the destination register, the value of the first register if the condition
/// is TRUE, and otherwise returns the value of the second source register NOTE: Use setCond(false, ...)
constexpr OPCodeTemplate CSELcondh_wD_wN_wM_t{0x1A800000U};

/// @brief 32bit Int CSEL when cond eq
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr CSELeq_wD_wN_wM_t{CSELcondh_wD_wN_wM_t | (0x00_U32 << 12_U32), ArgType::r32, ArgType::r32, ArgType::r32, false};

/// @brief CSEL Xd, Xn, Xm, cond: Returns, in the destination register, the value of the first register if the condition
/// is TRUE, and otherwise returns the value of the second source register NOTE: Use setCond(false, ...)
constexpr OPCodeTemplate CSELcondh_xD_xN_xM_t{0x9A800000U};
/// @brief 64bit Int CSEL when cond eq
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr CSELeq_xD_xN_xM_t{CSELcondh_xD_xN_xM_t, ArgType::r64, ArgType::r64, ArgType::r64, false};

/// @brief FCSEL Sd, Sn, Sm, cond: Returns, in the single-precision destination register, the value of the first
/// register if the condition is TRUE, and otherwise returns the value of the second source register NOTE: Use
/// setCond(false, ...)
constexpr OPCodeTemplate FCSELcondh_sD_sN_sM_t{0x1E200C00U};
/// @brief 32bit float FCSEL when cond eq
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FCSELeq_sD_sN_sM_t{FCSELcondh_sD_sN_sM_t, ArgType::r32f, ArgType::r32f, ArgType::r32f, false};
/// @brief FCSEL Dd, Dn, Dm, cond: Returns, in the double-precision destination register, the value of the first
/// register if the condition is TRUE, and otherwise returns the value of the second source register NOTE: Use
/// setCond(false, ...)
constexpr OPCodeTemplate FCSELcondh_dD_dN_dM_t{0x1E600C00U};
/// @brief 64bit float FCSEL when cond eq
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr FCSELeq_dD_dN_dM_t{FCSELcondh_dD_dN_dM_t, ArgType::r64f, ArgType::r64f, ArgType::r64f, false};

/// @brief CSINC Wd, Wn, Wm, cond: Returns, in the destination register, the value of the first source register if the
/// condition is TRUE, and otherwise returns the value of the second source register incremented by 1 NOTE: Use
/// setCond(false, ...)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate CSINCcondh_wD_wN_wM_t{0x1A800400U};
/// @brief CSINC Xd, Xn, Xm, cond: Returns, in the destination register, the value of the first source register if the
/// condition is TRUE, and otherwise returns the value of the second source register incremented by 1 NOTE: Use
/// setCond(false, ...)
constexpr OPCodeTemplate CSINCcondh_xD_xN_xM_t{0x9A800400U};

/// @brief CNT Vd.8B, Vn.8B: Counts the number of bits that have a value of one in each vector element in the source
/// register and places the result into a vector
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate CNT_vD8b_vN8b_t{0x0E205800U};
/// @brief UADDLV Hd, Vn.8B: Adds every vector element in the source register together and places the result into a
/// half-precision floating-point register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate UADDLV_hD_vN8b_t{0x2E303800U};
/// @brief FNEG Vd.2D, Vn.2D: Negates the value of each vector element in the source register and places the result into
/// a vector
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate FNEG_vD2d_vN2d_t{0x6EE0F800U};
/// @brief BIT Vd.16B, Vn.16B, Vm.16B: Inserts each bit from the first source register into the destination register if
/// the corresponding bit of the second source register is 1, otherwise leaves the bit in the destination register
/// unchanged
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate BIT_vD16b_vN16b_vM16b_t{0x6EA01C00U};
/// @brief: MOVI Vd.2D, 0: Places an immediate constant with value zero into every vector element of the destination
/// register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOVI_vD2d_0_t{0x6F00E400U};
/// @brief MOVI Vd.4S, 128, LSL 24: Places an immediate constant with value 128  24 into every vector element of the
/// destination register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOVI_vD4s_128lsl24_t{0x4F046400U};

/// @brief STR Wt, [Xn|SP, (Wm|Xm), LSL 2]: Calculates an address from a base register (left shifted by two bits) and an
/// offset register value and stores a word from a register to the calculated address.
constexpr OPCodeTemplate STR_wT_deref_xN_xMls2_t{0xB8207800U};

/// @brief LDR Wt, [Xn|SP{, pimm}]: Loads a word from memory and writes it to a register. The address that is used for
/// the load is calculated from a base register and a scaled, unsigned immediate offset (multiple of 4 in the range 0 to
/// 16380).
constexpr OPCodeTemplate LDR_wT_deref_xN_imm12zxls2_t{0xB9400000U};
/// @brief LDR Xt, [Xn|SP{, pimm}]: Loads a doubleword from memory and writes it to a register. The address that is used
/// for the load is calculated from a base register and a scaled, unsigned immediate offset (multiple of 8 in the range
/// 0 to 32760).
constexpr OPCodeTemplate LDR_xT_deref_xN_imm12zxls3_t{0xF9400000U};
/// @brief LDRB Wt, [Xn|SP{, pimm}]: Loads a byte from memory, zero-extends it and writes the result to a register. The
/// address that is used for the load is calculated from a base register and a scaled, unsigned immediate offset
/// (multiple of 1 in the range 0 to 4095).
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDRB_wT_deref_xN_imm12zx_t{0x39400000U};
/// @brief LDRH Wt, [Xn|SP{, pimm}]: Loads a halfword from memory, zero-extends it and writes the result to a register.
/// The address that is used for the load is calculated from a base register and a scaled, unsigned immediate offset
/// (multiple of 2 in the range 0 to 8190).
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDRH_wT_deref_xN_imm12zxls1_t{0x79400000U};
/// @brief LDRSB Wt, [Xn|SP{, pimm}]: Loads a byte from memory, sign-extends it to 32 bits, and writes the result to a
/// register. The address that is used for the load is calculated from a base register and a scaled, unsigned immediate
/// offset (in the range 0 to 4095).
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDRSB_wT_deref_xN_imm12zx_t{0x39C00000U};
/// @brief LDRSB Xt, [Xn|SP{, pimm}]: Loads a byte from memory, sign-extends it to 64 bits, and writes the result to a
/// register. The address that is used for the load is calculated from a base register and a scaled, unsigned immediate
/// offset (in the range 0 to 4095).
constexpr OPCodeTemplate LDRSB_xT_deref_xN_imm12zx_t{0x39800000U};
/// @brief LDRSH Wt, [Xn|SP{, pimm}]: Loads a halfword from memory, sign-extends it to 32 bits, and writes the result to
/// a register. The address that is used for the load is calculated from a base register and a scaled, unsigned
/// immediate offset (multiple of 2 in the range 0 to 8190).
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDRSH_wT_deref_xN_imm12zxls1_t{0x79C00000U};
/// @brief LDRSH Xt, [Xn|SP{, pimm}]: Loads a halfword from memory, sign-extends it to 64 bits, and writes the result to
/// a register. The address that is used for the load is calculated from a base register and a scaled, unsigned
/// immediate offset (multiple of 2 in the range 0 to 8190).
constexpr OPCodeTemplate LDRSH_xT_deref_xN_imm12zxls1_t{0x79800000U};
/// @brief LDRSW Xt, [Xn|SP{, pimm}]: Loads a word from memory, sign-extends it to 64 bits, and writes the result to a
/// register. The address that is used for the load is calculated from a base register and a scaled, unsigned immediate
/// offset (multiple of 4 in the range 0 to 16380).
constexpr OPCodeTemplate LDRSW_xT_deref_xN_imm12zxls2_t{0xB9800000U};

/// @brief LDR Xt, [Xn|SP{, simm}]!: Loads a doubleword from memory and writes it to a register. The address that is
/// used for the load is calculated from a base register and an unscaled, signed 9-bit immediate offset. The base
/// register is then incremented by the value of the immediate. (Post-index)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDR_xT_deref_xN_unscSImm9_postidx{0xF8400400U};
/// @brief STR Xt, [Xn|SP{, simm}]!: Stores a doubleword from a register to memory. The address that is used for the store is
/// calculated from a base register and an unscaled, signed 9-bit immediate offset. The base register is then
/// incremented by the value of the immediate. (Post-index)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STR_xT_deref_xN_unscSImm9_postidx{0xF8000400U};
/// @brief LDR Xt, [Xn|SP{, simm}]!: Loads a doubleword from memory and writes it to a register. The address that is
/// used for the load is calculated from a base register and an unscaled, signed 9-bit immediate offset. The base
/// register is then incremented by the value of the immediate. (Pre-index)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDR_xT_deref_xN_unscSImm9_preidx{0xF8400C00U};
/// @brief STR Xt, [Xn|SP{, simm}]!: Stores a doubleword from a register to memory. The address that is used for the store is
/// calculated from a base register and an unscaled, signed 9-bit immediate offset. The base register is then
/// incremented by the value of the immediate. (Pre-index)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STR_xT_deref_xN_unscSImm9_preidx{0xF8000C00U};
/// @brief LDRB Wt, [Xn|SP]{, simm}: Loads a byte from memory and writes it to a register. The address that is used for
/// the load is calculated from a base register and an unscaled, signed 9-bit immediate offset. The base register is
/// then incremented by the value of the immediate. (Post-index)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDRB_wT_deref_xN_unscSImm9_postidx{0x38400400U};
/// @brief STRB Wt, [Xn|SP]{, simm}: Stores a byte from a register to memory. The address that is used for the store is
/// calculated from a base register and an unscaled, signed 9-bit immediate offset. The base register is then
/// incremented by the value of the immediate. (Post-index)
constexpr OPCodeTemplate STRB_wT_deref_xN_unscSImm9_postidx{0x38000400U};
/// @brief LDRB Wt, [Xn|SP]{, simm}: Loads a byte from memory and writes it to a register. (Pre-index)
constexpr OPCodeTemplate LDRB_wT_deref_xN_unscSImm9_preidx{0x38400C00U};
/// @brief STRB Wt, [Xn|SP]{, simm}: Stores a byte from a register to memory. (Pre-index)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STRB_wT_deref_xN_unscSImm9_preidx{0x38000C00U};
/// @brief LDR Qt, [Xn|SP]{, simm}: Loads a quadword from memory and writes it to a SIMD register. The address that is
/// used for the store is calculated from a base register and an unscaled, signed 9-bit immediate offset. The base
/// register is then incremented by the value of the immediate. (Post-index)
constexpr OPCodeTemplate LDR_qT_deref_xN_unscSImm9_postidx{0x3CC00400U};
/// @brief STRB Qt, [Xn|SP]{, simm}: Stores a quadword from a SIMD register to memory. The address that is used for the
/// store is calculated from a base register and an unscaled, signed 9-bit immediate offset. The base register is then
/// incremented by the value of the immediate. (Post-index)
constexpr OPCodeTemplate STR_qT_deref_xN_unscSImm9_postidx{0x3C800400U};

/// @brief STR Wt, [Xn|SP{, simm}]!: Stores a word from a register to memory. The address that is used for the store is
/// calculated from a base register and an unscaled, signed 9-bit immediate offset. The base register is then
/// incremented by the value of the immediate. (Post-index)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STR_wT_deref_xN_unscSImm9_postidx{0xB8000400U};
/// @brief STR Wt, [Xn|SP, \#simm]!: Stores a word from a register to memory. The address that is used for the store is
/// calculated from a base register and an unscaled, signed 9-bit immediate offset. The base register is then
/// incremented by the value of the immediate. (Pre-index)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STR_wT_deref_xN_unscSImm9_preidx{0xB8000C00U};

/// @brief LDUR Wt, [Xn|SP{, simm}]: Calculates an address from a base register and an unscaled, signed immediate offset
/// (in the range -256 to 255), loads a word from memory, zero-extends it, and writes it to a register.
constexpr OPCodeTemplate LDUR_wT_deref_xN_unscSImm9_t{0xB8400000U};
/// @brief LDUR Xt, [Xn|SP{, simm}]: Calculates an address from a base register and an unscaled, signed immediate offset
/// (in the range -256 to 255), loads a word from memory, zero-extends it, and writes it to a register.
constexpr OPCodeTemplate LDUR_xT_deref_xN_unscSImm9_t{0xF8400000U};
/// @brief LDURB Wt, [Xn|SP{, simm}]: Calculates an address from a base register and an unscaled, signed immediate
/// offset (in the range -256 to 255), loads a byte from memory, zero-extends it, and writes it to a register
constexpr OPCodeTemplate LDURB_wT_deref_xN_unscSImm9_t{0x38400000U};
/// @brief LDURH Wt, [Xn|SP{, simm}]: Calculates an address from a base register and an unscaled, signed immediate
/// offset (in the range -256 to 255), loads a halfword from memory, zero-extends it, and writes it to a register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDURH_wT_deref_xN_unscSImm9_t{0x78400000U};
/// @brief LDURSB Wt, [Xn|SP{, simm}]: Calculates an address from a base register and an unscaled, signed immediate
/// offset (in the range -256 to 255), loads a byte from memory, sign-extends it, and writes it to a register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDURSB_wT_deref_xN_unscSImm9_t{0x38C00000U};
/// @brief LDURSB Xt, [Xn|SP{, simm}]: Calculates an address from a base register and an unscaled, signed immediate
/// offset (in the range -256 to 255), loads a byte from memory, sign-extends it, and writes it to a register
constexpr OPCodeTemplate LDURSB_xT_deref_xN_unscSImm9_t{0x38800000U};
/// @brief LDURSH Wt, [Xn|SP{, simm}]: Calculates an address from a base register and an unscaled, signed immediate
/// offset (in the range -256 to 255), loads a halfword from memory, sign-extends it, and writes it to a register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDURSH_wT_deref_xN_unscSImm9_t{0x78C00000U};
/// @brief LDURSH Xt, [Xn|SP{, simm}]: Calculates an address from a base register and an unscaled, signed immediate
/// offset (in the range -256 to 255), loads a halfword from memory, sign-extends it, and writes it to a register
constexpr OPCodeTemplate LDURSH_xT_deref_xN_unscSImm9_t{0x78800000U};
/// @brief LDURSW Xt, [Xn|SP{, simm}]: Calculates an address from a base register and an unscaled, signed immediate
/// offset (in the range -256 to 255), loads a word from memory, sign-extends it, and writes it to a register
constexpr OPCodeTemplate LDURSW_xT_deref_xN_unscSImm9_t{0xB8800000U};

/// @brief LDR Wt, [Xn|SP, (Wm|Xm): Calculates an address from a base register and an offset register value, loads a
/// word from memory, and writes it to a register.
constexpr OPCodeTemplate LDR_wT_deref_xN_xM_t{0xB8606800U};
/// @brief LDR Xt, [Xn|SP, (Wm|Xm): Calculates an address from a base register and an offset register value, loads a
/// doubleword from memory, and writes it to a register.
constexpr OPCodeTemplate LDR_xT_deref_xN_xM_t{0xF8606800U};
/// @brief LDR Wt, [Xn|SP, (Wm|Xm), LSL 2]: Calculates an address from a base register (left shifted by two bits) and an
/// offset register value, loads a word from memory, and writes it to a register.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDR_wT_deref_xN_xMls2_t{0xB8607800U};
/// @brief LDRB Wt, [Xn|SP, (Wm|Xm)]: Calculates an address from a base register and an offset register value, loads a
/// byte from memory, zero-extends it, and writes it to a register.
constexpr OPCodeTemplate LDRB_wT_deref_xN_xM_t{0x38606800U};
/// @brief LDRH Wt, [Xn|SP, (Wm|Xm)]: Calculates an address from a base register and an offset register value, loads a
/// halfword from memory, zero-extends it, and writes it to a register.
constexpr OPCodeTemplate LDRH_wT_deref_xN_xM_t{0x78606800U};
/// @brief LDRSB Wt, [Xn|SP, (Wm|Xm)]: Calculates an address from a base register and an offset register value, loads a
/// byte from memory, sign-extends it, and writes it to a register.
constexpr OPCodeTemplate LDRSB_wT_deref_xN_xM_t{0x38E06800U};
/// @brief LDRSB Xt, [Xn|SP, (Wm|Xm)]: Calculates an address from a base register and an offset register value, loads a
/// byte from memory, sign-extends it, and writes it to a register.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDRSB_xT_deref_xN_xM_t{0x38A06800U};
/// @brief LDRSH Wt, [Xn|SP, (Wm|Xm)]: Calculates an address from a base register and an offset register value, loads a
/// halfword from memory, sign-extends it, and writes it to a register.
constexpr OPCodeTemplate LDRSH_wT_deref_xN_xM_t{0x78E06800U};
/// @brief LDRSH Xt, [Xn|SP, (Wm|Xm)]: Calculates an address from a base register and an offset register value, loads a
/// halfword from memory, sign-extends it, and writes it to a register.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDRSH_xT_deref_xN_xM_t{0x78A06800U};
/// @brief LDRSW Xt, [Xn|SP, (Wm|Xm)]: Calculates an address from a base register and an offset register value, loads a
/// word from memory, sign-extends it, and writes it to a register.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDRSW_xT_deref_xN_xM_t{0xB8A06800U};

/// @brief STR Wt, [Xn|SP{, pimm}]: Stores a word from a register to memory. The address that is used for the store is
/// calculated from a base register and a scaled, unsigned immediate offset (multiple of 4 in the range 0 to 16380).
constexpr OPCodeTemplate STR_wT_deref_xN_imm12zxls2_t{0xB9000000U};
/// @brief STR Xt, [Xn|SP{, pimm}]: Stores a doubleword from a register to memory. The address that is used for the
/// store is calculated from a base register and a scaled, unsigned immediate offset (multiple of 8 in the range 0 to
/// 32760).
constexpr OPCodeTemplate STR_xT_deref_xN_imm12zxls3_t{0xF9000000U};
/// @brief STRB Wt, [Xn|SP{, pimm}]: Stores the least significant byte of a 32-bit register to memory. The address that
/// is used for the store is calculated from a base register and a scaled, unsigned immediate offset (in the range 0 to
/// 4095).
constexpr OPCodeTemplate STRB_wT_deref_xN_imm12zx_t{0x39000000U};
/// @brief STRH Wt, [Xn|SP{, pimm}]: Stores the least significant halfword of a 32-bit register to memory. The address
/// that is used for the store is calculated from a base register and a scaled, unsigned immediate offset (multiple of 2
/// in the range 0 to 8190).
constexpr OPCodeTemplate STRH_wT_deref_xN_imm12zxls1_t{0x79000000U};

/// @brief STUR Wt, [Xn|SP{, simm}]: Stores a 32-bit word to memory. The address that is used for the store is
/// calculated from a base register and an unscaled, signed immediate offset (in the range -256 to 255)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STUR_wT_deref_xN_unscSImm9_t{0xB8000000U};
/// @brief STUR Xt, [Xn|SP{, simm}]: Stores a 64-bit doubleword to memory. The address that is used for the store is
/// calculated from a base register and an unscaled, signed immediate offset (in the range -256 to 255)
constexpr OPCodeTemplate STUR_xT_deref_xN_unscSImm9_t{0xF8000000U};
/// @brief STUR St, [Xn|SP{, simm}]: Stores a single-precision floating-point register to memory. The address that is
/// used for the store is calculated from a base register and an unscaled, signed immediate offset (in the range -256 to
/// 255)
constexpr OPCodeTemplate STUR_sT_deref_xN_unscSImm9_t{0xBC000000U};
/// @brief STUR Dt, [Xn|SP{, simm}]: Stores a double-precision floating-point register to memory. The address that is
/// used for the store is calculated from a base register and an unscaled, signed immediate offset (in the range -256 to
/// 255)
constexpr OPCodeTemplate STUR_dT_deref_xN_unscSImm9_t{0xFC000000U};

/// @brief STURB Wt, [Xn|SP{, simm}]: Stores the least significant byte of a 32-bit register to memory. The address that
/// is used for the store is calculated from a base register and an unscaled, signed immediate offset (in the range -256
/// to 255)
constexpr OPCodeTemplate STURB_wT_deref_xN_unscSImm9_t{0x38000000U};
/// @brief STURH Wt, [Xn|SP{, simm}]: Stores the least significant halfword of a 32-bit register to memory. The address
/// that is used for the store is calculated from a base register and an unscaled, signed immediate offset (in the range
/// -256 to 255)
constexpr OPCodeTemplate STURH_wT_deref_xN_unscSImm9_t{0x78000000U};

/// @brief STR Wt, [Xn|SP, (Wm|Xm)]: Stores a 32-bit word register to memory. The address that is used for the store is
/// calculated from a base register and an unscaled, signed immediate offset (in the range -256 to 255)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STR_wT_deref_xN_xM_t{0xB8206800U};
/// @brief STR Xt, [Xn|SP, (Wm|Xm)]: Stores a 64-bit doubleword register to memory. The address that is used for the
/// store is calculated from a base register and an unscaled, signed immediate offset (in the range -256 to 255)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STR_xT_deref_xN_xM_t{0xF8206800U};
/// @brief STRB Wt, [Xn|SP, (Wm|Xm)]: Stores the least significant byte of a 32-bit register to memory. The address that
/// is used for the store is calculated from a base register and an unscaled, signed immediate offset (in the range -256
/// to 255)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STRB_wT_deref_xN_xM_t{0x38206800U};
/// @brief STRH Wt, [Xn|SP, (Wm|Xm)]: Stores the least significant halfword of a 32-bit register to memory. The address
/// that is used for the store is calculated from a base register and an unscaled, signed immediate offset (in the range
/// -256 to 255)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STRH_wT_deref_xN_xM_t{0x78206800U};

/// @brief LDP Dt1, Dt2, [Xn|SP], imm: Loads two double-precision floating-point values from memory and writes them to
/// two registers. The address that is used for the load is calculated from a base register and a scaled, signed
/// immediate offset (mutiple of 8 in the range -512 to 504, encoded in imm7 as imm/8).  The base register is then
/// incremented by the value of the immediate. (Post-index)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDP_dT1_dT2_deref_xN_scSImm7_postidx_t{0x6CC00000U};
/// @brief STP Xt1, Xt2, [Xn|SP], imm: Stores two double-precision floating-point values to memory. The address that is
/// used for the store is calculated from a base register and a scaled, signed immediate offset (multiple of 8 in the
/// range -512 to 504, encoded in imm7 as imm/8). The base register is then incremented by the value of the immediate.
/// (Post-index)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STP_dT1_dT2_deref_xN_scSImm7_postidx_t{0x6C800000U};
/// @brief LDP Dt1, Dt2, [Xn|SP], imm: Loads two double-precision floating-point values from memory and writes them to
/// two registers. The base register is incremented by a scaled, signed immediate offset (mutiple of 8 in the range -512
/// to 504, encoded in imm7 as imm/8). Then the address that is used for the load is updated base register. (Pre-index)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDP_dT1_dT2_deref_xN_scSImm7_preidx_t{0x6DC00000U};
/// @brief STP Xt1, Xt2, [Xn|SP], imm: Stores two double-precision floating-point values to memory. The base register is
/// incremented by a scaled, signed immediate offset (mutiple of 8 in the range -512 to 504, encoded in imm7 as imm/8).
/// Then the address that is used for the store is updated base register. (Pre-index)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STP_dT1_dT2_deref_xN_scSImm7_preidx_t{0x6D800000U};

/// @brief LDP Wt1, Wt2, [Xn|SP{, imm}]: Loads two 64-bit doublewords from memory and writes them to two registers. The
/// address that is used for the load is calculated from a base register and a scaled, signed immediate offset (mutiple
/// of 8 in the range -512 to 504, encoded in imm7 as imm/8).
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDP_wT1_wT2_deref_xN_scSImm7_t{0x29400000U};
/// @brief LDP Xt1, Xt2, [Xn|SP{, imm}]: Loads two 64-bit doublewords from memory and writes them to two registers. The
/// address that is used for the load is calculated from a base register and a scaled, signed immediate offset (mutiple
/// of 8 in the range -512 to 504, encoded in imm7 as imm/8).
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDP_xT1_xT2_deref_xN_scSImm7_t{0xA9400000U};
/// @brief STP Wt1, Wt2, [Xn|SP{, imm}]: Stores two 32-bit words to memory. The address that is used for the store is
/// calculated from a base register and a scaled, signed immediate offset (multiple of 8 in the range -256 to 252,
/// encoded in imm7 as imm/4)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STP_wT1_wT2_deref_xN_scSImm7_t{0x29000000U};
/// @brief STP Dt1, Dt2, [Xn|SP{, imm}]: Stores two 64-bit doublewords to memory. The address that is used for the store
/// is calculated from a base register and a scaled, signed immediate offset (multiple of 8 in the range -512 to 504,
/// encoded in imm7 as imm/8)
constexpr OPCodeTemplate STP_xT1_xT2_deref_xN_scSImm7_t{0xA9000000U};
/// @brief LDP Dt1, Dt2, [Xn|SP{, imm}]: Loads two double-precision floating-point values from memory and writes them to
/// two registers. The address that is used for the load is calculated from a base register and a scaled, signed
/// immediate offset (mutiple of 8 in the range -512 to 504, encoded in imm7 as imm/8).
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDP_dT1_dT2_deref_xN_scSImm7_t{0x6D400000U};
/// @brief STP Xt1, Xt2, [Xn|SP{, imm}]: Stores two double-precision floating-point values to memory. The address that
/// is used for the store is calculated from a base register and a scaled, signed immediate offset (multiple of 8 in the
/// range -512 to 504, encoded in imm7 as imm/8)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STP_dT1_dT2_deref_xN_scSImm7_t{0x6D000000U};

/// @brief STP Dt1, Dt2, [Xn|SP{, imm}]: Stores two 64-bit doublewords to memory. The address that is used for the store
/// is calculated from a base register and a scaled, signed immediate offset (multiple of 8 in the range -512 to 504,
/// encoded in imm7 as imm/8) (Post-index)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STP_xT1_xT2_deref_xN_scSImm7_postidx_t{0xA8800000U};

/// @brief LDR St, [Xn|SP{, pimm}]: Loads a single-precision floating-point value from memory. The address that is used
/// for the store is calculated from a base register and a scaled, unsigned immediate offset (multiple of 4 in the range
/// 0 to 16380).
constexpr OPCodeTemplate LDR_sT_deref_xN_imm12zxls2_t{0xBD400000U};
/// @brief LDR Dt, [Xn|SP{, pimm}]: Loads a double-precision floating-point value from memory. The address that is used
/// for the store is calculated from a base register and a scaled, unsigned immediate offset (multiple of 8 in the range
/// 0 to 32760).
constexpr OPCodeTemplate LDR_dT_deref_xN_imm12zxls3_t{0xFD400000U};

/// @brief LDR St, [Xn|SP, (Wm|Xm)]: Loads a single-precision floating-point value from memory. The address that is used
/// for the store is calculated from a base register and a scaled, unsigned immediate offset (multiple of 4 in the range
/// 0 to 16380).
constexpr OPCodeTemplate LDR_sT_deref_xN_xM_t{0xBC606800U};
/// @brief LDR Dt, [Xn|SP, (Wm|Xm)]: Loads a double-precision floating-point value from memory. The address that is used
/// for the store is calculated from a base register and a scaled, unsigned immediate offset (multiple of 8 in the range
/// 0 to 32760).
constexpr OPCodeTemplate LDR_dT_deref_xN_xM_t{0xFC606800U};

/// @brief LDUR St, [Xn|SP{, simm}]: Loads a single-precision floating-point value from memory. The address that is used
/// for the store is calculated from a base register and an unscaled, signed immediate offset (in the range -256 to
/// 255).
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDUR_sT_deref_xN_unscSImm9_t{0xBC400000U};
/// @brief LDUR Dt, [Xn|SP{, simm}]: Loads a double-precision floating-point value from memory. The address that is used
/// for the store is calculated from a base register and an unscaled, signed immediate offset (in the range -256 to
/// 255).
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDUR_dT_deref_xN_unscSImm9_t{0xFC400000U};

/// @brief STR St, [Xn|SP{, pimm}]: Stores a single-precision floating-point value from a register to memory. The
/// address that is used for the store is calculated from a base register and a scaled, unsigned immediate offset
/// (multiple of 4 in the range 0 to 16380).
constexpr OPCodeTemplate STR_sT_deref_xN_imm12zxls2_t{0xBD000000U};
/// @brief STR Dt, [Xn|SP{, pimm}]: Stores a double-precision floating-point value from a register to memory. The
/// address that is used for the store is calculated from a base register and a scaled, unsigned immediate offset
/// (multiple of 8 in the range 0 to 32760).
constexpr OPCodeTemplate STR_dT_deref_xN_imm12zxls3_t{0xFD000000U};

/// @brief STR St, [Xn|SP, (Wm|Xm)]: Stores a single-precision floating-point value from a register to memory. The
/// address that is used for the store is calculated from a base register value and an offset register value.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STR_sT_deref_xN_xM_t{0xBC206800U};
/// @brief STR Dt, [Xn|SP, (Wm|Xm)]: Stores a double-precision floating-point value from a register to memory. The
/// address that is used for the store is calculated from a base register value and an offset register value.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STR_dT_deref_xN_xM_t{0xFC206800U};

/// @brief MOV Wd, Wm: Copies the value in a source register to the destination register
constexpr OPCodeTemplate MOV_wD_wM_t{0x2A0003E0U};
/// @brief MOV Xd, Xm: Copies the value in a source register to the destination register
constexpr OPCodeTemplate MOV_xD_xM_t{0xAA0003E0U};

/// @brief ADR Xd, label: Adds an immediate value (in the range +-1MB, encoded as 21 bits) to the PC value to form a
/// PC-relative address
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate ADR_xD_signedOffset21_t{0x10000000U};

/// @brief CMN Wn|WSP, imm{, shift}: Adds a register value and an optionally-shifted (by 12 bits) immediate value and
/// updates the condition flags.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate CMN_wN_imm12zxols12_t{0x3100001FU};
/// @brief CMN Xn|SP, imm{, shift}: Adds a register value and an optionally-shifted (by 12 bits) immediate value and
/// updates the condition flags.
constexpr OPCodeTemplate CMN_xN_imm12zxols12_t{0xB100001FU};

/// @brief MRS Xt, NZCV: Read the NZCV AArch64 System register into a general-purpose register
constexpr OPCodeTemplate MRS_xT_NZCV{0xD53B4200U};
/// @brief MRS Xt, NZCV: Write the NZCV AArch64 System register from a general-purpose register
constexpr OPCodeTemplate MSR_NZCV_xT{0xD51B4200U};

/// @brief MRS Xt, cntvct_el0: Read the cntvct_el0 AArch64 System register into a general-purpose register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MRS_xT_CNTVCT_EL0{0xD53BE040U};

/// @brief Conditional Set sets the destination register to 1 if the condition is TRUE, and otherwise sets it to 0.
/// The equivalent instruction is CSINC Wd, WZR, WZR, invert(cond).
/// NOTE: Use setCond(false, ...)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate CSET_wD{0x1A9F07E0U};

///
/// @brief Instruction generator for LDR so the inputs can be comfortably switched in a single line
///
/// @param isGPR Whether a GPR or an FPR should be loaded
/// @param is64 Target register size
/// @return OPCodeTemplate Resulting OPCodeTemplate
inline constexpr OPCodeTemplate LDR_T_deref_N_scUImm12(bool const isGPR, bool const is64) VB_NOEXCEPT {
  if (isGPR) {
    return is64 ? LDR_xT_deref_xN_imm12zxls3_t : LDR_wT_deref_xN_imm12zxls2_t;
  }
  return is64 ? LDR_dT_deref_xN_imm12zxls3_t : LDR_sT_deref_xN_imm12zxls2_t;
}

///
/// @brief Instruction generator for STR so the inputs can be comfortably switched in a single line
///
/// @param isGPR Whether a GPR or an FPR should be stored
/// @param is64 Target register size
/// @return OPCodeTemplate Resulting OPCodeTemplate
inline constexpr OPCodeTemplate STR_T_deref_N_scUImm12(bool const isGPR, bool const is64) VB_NOEXCEPT {
  if (isGPR) {
    return is64 ? STR_xT_deref_xN_imm12zxls3_t : STR_wT_deref_xN_imm12zxls2_t;
  }
  return is64 ? STR_dT_deref_xN_imm12zxls3_t : STR_sT_deref_xN_imm12zxls2_t;
}

} // namespace aarch64
} // namespace vb

#endif

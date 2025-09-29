///
/// @file tricore_encoding.hpp
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
#ifndef TRICORE_ENCODING_HPP
#define TRICORE_ENCODING_HPP

#include "src/config.hpp"
#ifdef JIT_TARGET_TRICORE

#include <cstdint>

#include "src/core/common/util.hpp"

namespace vb {
namespace tc {

///
/// @brief Native registers and their encoding that can be placed into the respective fields in an instruction
/// NOTE: REG::NONE will be used to represent an invalid register (or no register at all)
///
// coverity[autosar_cpp14_a7_2_4_violation] autosar required style is more bug prone
enum class REG : uint32_t { // clang-format off
  D0, D1, D2, D3, D4, D5, D6, D7, D8, D9, D10, D11, D12, D13, D14, D15,
  A0 = 0b0001'0000U, A1, A2, A3, A4, A5, A6, A7, A8, A9, A10, A11, A12, A13, A14, A15,
  NUMREGS,
  SP = A10, RA = A11,
  NONE = 0b1000'0000U
}; // clang-format on

constexpr uint32_t totalNumRegs{static_cast<uint32_t>(REG::NUMREGS)}; ///< Total number of registers in the enum

namespace RegUtil {

///
/// @brief Checks whether a register is a data register (as opposed to an address register)
///
/// @param reg Register to check
/// @return bool Whether the register is a data register
inline constexpr bool isDATA(REG const reg) VB_NOEXCEPT {
  return (static_cast<uint32_t>(reg) & 0b0001'0000U) == 0U;
}

///
/// @brief Whether a register can be an extended register (i.e. the register is an even one)
///
/// @param reg Input register
/// @return Whether a register can be an extended register
///
inline constexpr bool canBeExtReg(REG const reg) VB_NOEXCEPT {
  return (static_cast<uint32_t>(reg) & 0b1U) == 0U;
}

///
/// @brief Get the other register of an extended register holding the 32 most significant bits, returns the primary if
/// the secondary is passed and vice versa
///
/// @param reg Data register
/// @return REG Secondary register of extended register
inline constexpr REG getOtherExtReg(REG const reg) VB_NOEXCEPT {
  assert(isDATA(reg) && "Register is not a data register");
  // coverity[autosar_cpp14_a7_2_1_violation]
  return static_cast<REG>(static_cast<uint32_t>(reg) ^ 1U);
}

///
/// @brief Get the other register of an extended register holding the 32 most significant bits, returns the primary if
/// the secondary is passed and vice versa
///
/// @param reg Data register
/// @return REG Secondary register of extended register
inline constexpr REG getOtherExtAddrReg(REG const reg) VB_NOEXCEPT {
  assert(!isDATA(reg) && "Register is not a address register");
  // coverity[autosar_cpp14_a7_2_1_violation]
  return static_cast<REG>(static_cast<uint32_t>(reg) ^ 1U);
}

///
/// @brief Checks whether a register is a general purpose register
///
/// @param reg Register to check
/// @return bool Whether the register is a general purpose register
constexpr inline bool isGPR(REG const reg) VB_NOEXCEPT {
  static_cast<void>(reg);
  return true;
}

} // namespace RegUtil

///
/// @brief Comparison flags for instructions
///
enum class CMPFFLAGS : uint32_t {
  LT = 1U,      // Bit 0: less than
  EQ = 2U,      // Bit 1: equal
  GT = 4U,      // Bit 2: greater than
  UNORD = 8U,   // Bit 3: unordered
  A_SUBN = 16U, // Bit 4: First arg was subnormal
  B_SUBN = 32U  // Bit 5: Second arg was subnormal
};

///
/// @brief Consolidate multiple comparison flags for instructions into one
///
/// @param lhs First comparison flag
/// @param rhs Second comparison flag
/// @return Resulting comparison flag
///
inline CMPFFLAGS operator|(const CMPFFLAGS lhs, const CMPFFLAGS rhs) VB_NOEXCEPT {
  // coverity[autosar_cpp14_a7_2_1_violation]
  return static_cast<CMPFFLAGS>(static_cast<uint32_t>(lhs) | static_cast<uint32_t>(rhs));
}

///
/// @brief Abstract definition for the input argument of an abstract instruction
///
/// NOTE: Only the operand types supported by the current selectInstr are listed here
///
// coverity[autosar_cpp14_a7_2_4_violation] autosar required style is more bug prone
enum class ArgType : uint8_t { // clang-format off
  NONE = 0b00000000,
  I32 = 0b01000000, addrReg32, d15, dataReg32_a, dataReg32_b, dataReg32_c,
  const4sx_32, const8zx_32, const9sx_32, const9zx_32, const16sx_32,
  I64 = 0b10000000, addrReg64, dataReg64,
  TYPEMASK = 0b11000000
}; // clang-format on

///
/// @brief Basic template for TriCore OPCodes
///
using OPCodeTemplate = uint32_t;

///
/// @brief Complete description of an TriCore instruction
///
/// This includes an opcode template, the destination and source types and whether the sources are commutative
/// NOTE: For readonly instructions like CMP, dstType is ArgType::NONE, for instructions only taking a single input,
/// src1Type is ArgType::NONE. Commutation of source inputs is designed in such a way that an instruction is considered
/// source-commutative if the data in the destination after execution is the same if the source inputs are swapped
///
struct AbstrInstr final {
  OPCodeTemplate opcode;    ///< Basic opcode template
  ArgType destType;         ///< Destination type
  ArgType src0Type;         ///< First source type
  ArgType src1Type;         ///< Second source type
  bool src_0_1_commutative; ///< Whether first and second source are commutative
  bool src0_dst_same;       ///< Whether require first source and destination to be same
  bool useD15;              ///< Whether require use d15 as implicit register
};

/// @brief CLZ  D[c], D[a]: Count Leading Zeros
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__CLZ_Dc_Da{0x01B0000FU, ArgType::dataReg32_c, ArgType::dataReg32_a, ArgType::NONE, false, false, false};
/// @brief POPCNT.W  D[c], D[a]: Count population (ones) in register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__POPCNTW_Dc_Da{0x0220004BU, ArgType::dataReg32_c, ArgType::dataReg32_a, ArgType::NONE, false, false, false};

/// @brief ADDI  D[c], D[a], const16: Add Immediate
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__ADDI_Dc_Da_const16sx{0x0000001BU, ArgType::dataReg32_c, ArgType::dataReg32_a, ArgType::const16sx_32, true, false, false};
/// @brief ADDIH  D[c], D[a], const16: Add Immediate High
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__ADDIH_Dc_Da_const16sx{0x0000009BU, ArgType::dataReg32_c, ArgType::dataReg32_a, ArgType::const16sx_32, true, false, false};
/// @brief ADD  D[c], D[a], const9: Add
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__ADD_Dc_Da_const9sx{0x0000008BU, ArgType::dataReg32_c, ArgType::dataReg32_a, ArgType::const9sx_32, true, false, false};
/// @brief ADD  D[c], D[a], D[b]: Add
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__ADD_Dc_Da_Db{0x0000000BU, ArgType::dataReg32_c, ArgType::dataReg32_a, ArgType::dataReg32_b, true, false, false};
/// @brief ADD  D[a], D[15], const4: Add (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__ADD_Da_D15_const4sx{0x0092U, ArgType::dataReg32_a, ArgType::d15, ArgType::const4sx_32, true, false, true};
/// @brief ADD  D[15], D[a], const4: Add (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__ADD_D15_Da_const4sx{0x009AU, ArgType::d15, ArgType::dataReg32_a, ArgType::const4sx_32, true, false, true};
/// @brief ADD  D[a], const4: Add (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__ADD_Da_const4sx{0x00C2U, ArgType::dataReg32_a, ArgType::dataReg32_a, ArgType::const4sx_32, true, true, false};
/// @brief ADD  D[a], D[15], D[b]: Add (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__ADD_Da_D15_Db{0x0012U, ArgType::dataReg32_a, ArgType::d15, ArgType::dataReg32_b, true, false, true};
/// @brief ADD  D[15], D[a], D[b]: Add (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__ADD_D15_Da_Db{0x001AU, ArgType::d15, ArgType::dataReg32_a, ArgType::dataReg32_b, true, false, true};
/// @brief ADD  D[a], D[b]: Add (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__ADD_Da_Db{0x0042U, ArgType::dataReg32_a, ArgType::dataReg32_a, ArgType::dataReg32_b, true, true, false};

/// @brief AND  D[c], D[a], const9: Bitwise AND
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__AND_Dc_Da_const9zx{0x0100008FU, ArgType::dataReg32_c, ArgType::dataReg32_a, ArgType::const9zx_32, true, false, false};
/// @brief AND  D[c], D[a], D[b]: Bitwise AND
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__AND_Dc_Da_Db{0x0080000FU, ArgType::dataReg32_c, ArgType::dataReg32_a, ArgType::dataReg32_b, true, false, false};
/// @brief AND  D[15], const8: Bitwise AND (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__AND_D15_const8zx{0x0016U, ArgType::d15, ArgType::d15, ArgType::const8zx_32, true, true, true};
/// @brief AND  D[a], D[b]: Bitwise AND (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__AND_Da_Db{0x0026U, ArgType::dataReg32_a, ArgType::dataReg32_a, ArgType::dataReg32_b, true, true, false};

/// @brief OR  D[c], D[a], const9: Bitwise OR
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__OR_Dc_Da_const9zx{0x0140008FU, ArgType::dataReg32_c, ArgType::dataReg32_a, ArgType::const9zx_32, true, false, false};
/// @brief OR  D[c], D[a], D[b]: Bitwise OR
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__OR_Dc_Da_Db{0x00A0000FU, ArgType::dataReg32_c, ArgType::dataReg32_a, ArgType::dataReg32_b, true, false, false};
/// @brief OR  D[15], const8: Bitwise OR (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__OR_D15_const8zx{0x0096U, ArgType::d15, ArgType::d15, ArgType::const8zx_32, true, true, true};
/// @brief OR  D[a], D[b]: Bitwise OR (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__OR_Da_Db{0x00A6U, ArgType::dataReg32_a, ArgType::dataReg32_a, ArgType::dataReg32_b, true, true, false};

/// @brief SUB  D[c], D[a], D[b]: Subtract
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__SUB_Dc_Da_Db{0x0080000BU, ArgType::dataReg32_c, ArgType::dataReg32_a, ArgType::dataReg32_b, false, false, false};
/// @brief SUB  D[c], D[15], D[b]: Subtract (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__SUB_Dc_D15_Db{0x0052U, ArgType::dataReg32_c, ArgType::d15, ArgType::dataReg32_b, false, false, true};
/// @brief SUB  D[15], D[a], D[b]: Subtract (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__SUB_D15_Da_Db{0x005AU, ArgType::d15, ArgType::dataReg32_a, ArgType::dataReg32_b, false, false, true};
/// @brief SUB  D[a], D[b]: Subtract (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__SUB_Da_Db{0x00A2U, ArgType::dataReg32_a, ArgType::dataReg32_a, ArgType::dataReg32_b, false, true, false};

/// @brief MUL  D[c], D[a], const9: Multiply
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__MUL_Dc_Da_const9sx{0x00200053U, ArgType::dataReg32_c, ArgType::dataReg32_a, ArgType::const9sx_32, true, false, false};
/// @brief MUL  D[c], D[a], D[b]: Multiply
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__MUL_Dc_Da_Db{0x000A0073U, ArgType::dataReg32_c, ArgType::dataReg32_a, ArgType::dataReg32_b, true, false, false};
/// @brief MUL  D[a], D[b]: Multiply (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__MUL_Da_Db{0x00E2U, ArgType::dataReg32_a, ArgType::dataReg32_a, ArgType::dataReg32_b, true, true, false};

/// @brief RSUB  D[c], D[a], const9: Reverse Subtract
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__RSUB_Dc_Da_const9sx{0x0100008BU, ArgType::dataReg32_c, ArgType::dataReg32_a, ArgType::const9sx_32, false, false, false};
/// @brief RSUB  D[a]: Reverse Subtract(16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__RSUB_Da{0x5032U, ArgType::dataReg32_a, ArgType::dataReg32_a, ArgType::NONE, false, true, false};

/// @brief SH  D[c], D[a], const9: Shift
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__SH_Dc_Da_const9sx{0x0000008FU, ArgType::dataReg32_c, ArgType::dataReg32_a, ArgType::const9sx_32, false, false, false};
/// @brief SH  D[c], D[a], D[b]: Shift
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__SH_Dc_Da_Db{0x0000000FU, ArgType::dataReg32_c, ArgType::dataReg32_a, ArgType::dataReg32_b, false, false, false};
/// @brief SH  D[a], const4: Shift (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__SH_Da_const4sx{0x0006U, ArgType::dataReg32_a, ArgType::dataReg32_a, ArgType::const4sx_32, false, true, false};

/// @brief SHA  D[c], D[a], const9: Arithmetic Shift
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__SHA_Dc_Da_const9sx{0x0020008FU, ArgType::dataReg32_c, ArgType::dataReg32_a, ArgType::const9sx_32, false, false, false};
/// @brief SHA  D[c], D[a], D[b]: Arithmetic Shift
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__SHA_Dc_Da_Db{0x0010000FU, ArgType::dataReg32_c, ArgType::dataReg32_a, ArgType::dataReg32_b, false, false, false};
/// @brief SHA  D[c], D[a], D[b]: Arithmetic Shift (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__SHA_Da_const4sx{0x0086U, ArgType::dataReg32_a, ArgType::dataReg32_a, ArgType::const4sx_32, false, true, false};

/// @brief XOR  D[c], D[a], const9: Bitwise XOR
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__XOR_Dc_Da_const9zx{0x0180008FU, ArgType::dataReg32_c, ArgType::dataReg32_a, ArgType::const9zx_32, true, false, false};
/// @brief XOR  D[c], D[a], D[b]: Bitwise XOR
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__XOR_Dc_Da_Db{0x00C0000FU, ArgType::dataReg32_c, ArgType::dataReg32_a, ArgType::dataReg32_b, true, false, false};
/// @brief XOR  D[a], D[b]: Bitwise XOR (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr I__XOR_Da_Db{0x00C6U, ArgType::dataReg32_a, ArgType::dataReg32_a, ArgType::dataReg32_b, true, true, false};

///
/// @brief Check whether the given instruction is a 16-bit instruction (32-bit encoding otherwise)
///
/// @param opcode Instruction to check
/// @return boolean
///
inline bool is16BitInstr(OPCodeTemplate const opcode) VB_NOEXCEPT {
  return (static_cast<uint32_t>(opcode) & 0b1U) == 0b0U;
}

// 16-bit instruction abbreviations
// const4:  15-12  4-bit constant
// const8:  15-08  9-bit constant
// d:       11-08  Destination register
// disp4:   11-08  4-bit displacement
// disp8:   15-08  8-bit displacement
// n:       15-12  Address shift value in add scale
// off4:    15-12  4-bit offset
// off4sro: 11-08  4-bit offset (SRO)

// 16-bit instruction decorators
// zx: zero extend
// sx: signed extend
// ls2: left shift 2

/// Nop instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
// coverity[single_use]
constexpr OPCodeTemplate NOP{0x0000U};

/// Jump and link instruction
constexpr OPCodeTemplate JL_disp24sx2{0x0000005DU};
/// Jump instruction
constexpr OPCodeTemplate J_disp24sx2{0x0000001DU};
/// Jump and link absolute instruction
constexpr OPCodeTemplate JLA_absdisp24sx2{0x000000DDU};
/// Jump absolute instruction
constexpr OPCodeTemplate JA_absdisp24sx2{0x0000009DU};
/// Jump and link indirect instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate JLI_Aa{(0x02_U32 << 20_U32) | 0x2D_U32};
/// Jump indirect instruction (16b instruction)
constexpr OPCodeTemplate JI_Aa{0x00DCU};

/// Jump if register not equal to constant and decrement register
constexpr OPCodeTemplate JNED_Da_const4sx_disp15sx2{(0x1_U32 << 31_U32) | 0x9F_U32};

/// Jump if register not equal to zero and decrement register
constexpr OPCodeTemplate LOOP_Ab_disp15sx2{0x000000FDU};

/// Jump if register equal to constant instruction
constexpr OPCodeTemplate JEQ_Da_const4sx_disp15sx2{0x000000DFU};
/// Jump if register equal to register instruction
constexpr OPCodeTemplate JEQ_Da_Db_disp15sx2{0x0000005FU};
/// Jump if register not equal to constant instruction
constexpr OPCodeTemplate JNE_Da_const4sx_disp15sx2{0x800000DFU};
/// Jump if register not equal to register instruction
constexpr OPCodeTemplate JNE_Da_Db_disp15sx2{0x8000005FU};

/// Jump if register less than signed constant instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate JLT_Da_const4sx_disp15sx2{0x000000BFU};
/// Jump if register less than signed register instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate JLT_Da_Db_disp15sx2{0x0000003FU};
/// Jump if register less than unsigned constant instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate JLTU_Da_const4zx_disp15sx2{0x800000BFU};
/// Jump if register less than unsigned register instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate JLTU_Da_Db_disp15sx2{0x8000003FU};

/// Jump if register greater or equal than signed constant instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate JGE_Da_const4sx_disp15sx2{0x000000FFU};
/// Jump if register greater or equal than signed register instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate JGE_Da_Db_disp15sx2{0x0000007FU};
/// Jump if register greater or equal than unsigned constant instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate JGEU_Da_const4zx_disp15sx2{0x800000FFU};
/// Jump if register greater or equal than unsigned register instruction
constexpr OPCodeTemplate JGEU_Da_Db_disp15sx2{0x8000007FU};

/// Jump if register less than zero (16-bit instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate JLTZ_Db_disp4zx2{0x000EU};
/// Jump if register is not zero (16-bit instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate JNZ_Db_disp4zx2{0x00F6U};

/// Jump if address register is zero instruction
constexpr OPCodeTemplate JZA_Aa_disp15sx2{0x000000BDU};
/// Jump if address register is not zero instruction
constexpr OPCodeTemplate JNZA_Aa_disp15sx2{0x800000BDU};
/// Jump if address registers are equal instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate JEQA_Aa_Ab_disp15sx2{0x0000007DU};
/// Jump if address registers are not equal instruction
constexpr OPCodeTemplate JNEA_Aa_Ab_disp15sx2{0x8000007DU};

/// Jump if bit n of register is zero instruction
constexpr OPCodeTemplate JZT_Da_n_disp15sx2{0x0000006FU};
/// Jump if bit n of register is not zero instruction
constexpr OPCodeTemplate JNZT_Da_n_disp15sx2{0x8000006FU};

/// Call absolute instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate CALLA_absdisp24sx2{0x000000EDU};
/// Fast call instruction
/// NOTE: Before using FCALL, checking stack size is unnecessary, since we reserve 64B stack size in @b
/// Runtime::setStackFence.
constexpr OPCodeTemplate FCALL_disp24sx2{0x00000061U};
/// Fast call absolute instruction
constexpr OPCodeTemplate FCALLA_disp24sx2{0x000000E1U};
/// Fast call indirect instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate FCALLI_Aa{0x0010002DU};
/// Call indirect instruction
constexpr OPCodeTemplate CALLI_Aa{(0x00_U32 << 20_U32) | 0x2D_U32};
/// Fast return instruction
constexpr OPCodeTemplate FRET{0x7000U};
/// Return instruction
constexpr OPCodeTemplate RET{(0x06_U32 << 22_U32) | 0x0D_U32};
/// Load effective address instruction
constexpr OPCodeTemplate LEA_Aa_deref_Ab_off16sx{0x000000D9U};

/// Insert bit field from register instruction
constexpr OPCodeTemplate INSERT_Dc_Da_Db_pos_width{0x00000037U};
/// Insert bit field from constant instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate INSERT_Dc_Da_const4_pos_width{0x000000B7U};

/// Extract bit field signed instruction
constexpr OPCodeTemplate EXTR_Dc_Da_pos_width{(0x2_U32 << 21_U32) | 0x37_U32};
/// Extract bit field unsigned instruction
constexpr OPCodeTemplate EXTRU_Dc_Da_pos_width{(0x3_U32 << 21_U32) | 0x37_U32};

/// Extract from double register instruction
constexpr OPCodeTemplate DEXTR_Dc_Da_Db_pos{0x00000077U};
/// Extract from double register instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate DEXTR_Dc_Da_Db_Dd{(0x4_U32 << 21_U32) | 0x17_U32};

/// Load byte signed instruction
constexpr OPCodeTemplate LDB_Da_deref_Ab_off16sx{0x00000079U};
/// Load byte unsigned instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDBU_Da_deref_Ab_off16sx{0x00000039U};
/// Load byte unsigned (Pre-increment addressing mode) instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDBU_Da_deref_Ab_off10sx_preinc{(0x11_U32 << 22_U32) | 0x09_U32};
/// Load byte unsigned (Post-increment addressing mode) instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDBU_Da_deref_Ab_off10sx_postinc{(0x01_U32 << 22_U32) | 0x09_U32};
/// Load byte unsigned (Post-increment addressing mode) instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDBU_Dc_deref_Ab_postinc{0x0004U};
/// Load byte unsigned instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDBU_Dc_deref_Ab{0x0014U};
/// Load byte instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDBU_D15_deref_Ab_off4srozx{0x000CU};
/// Load byte instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDBU_Dc_deref_A15_off4zx{0x0008U};
/// Load halfword signed instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDH_Da_deref_Ab_off16sx{0x000000C9U};
/// Load halfword signed instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDH_D15_deref_Ab_off4srozxls1{0x008CU};
/// Load halfword signed instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDH_Dc_deref_A15_off4zxls1{0x0088U};
/// Load halfword instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDH_Dc_deref_Ab{0x0094U};
/// Load halfword unsigned instruction
constexpr OPCodeTemplate LDHU_Da_deref_Ab_off16sx{0x000000B9U};
/// Load word instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDW_Da_deref_Ab_off16sx{0x00000019U};
/// Load word instruction (Pre-increment addressing mode)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDW_Da_deref_Ab_off10sx_preinc{(0x14_U32 << 22_U32) | 0x09_U32};
/// Load word instruction (16b instruction)
constexpr OPCodeTemplate LDW_Dc_deref_Ab{0x0054U};
/// Load word instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDW_Dc_deref_A15_off4zxls2{0x0048U};
/// Load word instruction (16b instruction)
constexpr OPCodeTemplate LDW_D15_deref_A10_const8zxls2{0x0058U};
/// Load word instruction (16b instruction)
constexpr OPCodeTemplate LDW_D15_deref_Ab_off4srozxls2{0x004CU};
/// LD.WD[c], A[b] (SLR)(Post-increment Addressing Mode) D[c] = M(A[b], word);A[b] = A[b] + 4;
constexpr OPCodeTemplate LDW_Dc_deref_Ab_postinc{0x0044U};

/// Load doubleword (Post-increment addressing mode) instruction
constexpr OPCodeTemplate LDW_Da_deref_Ab_off10sx_postinc{(0x04_U32 << 22_U32) | 0x09_U32};
/// Load doubleword instruction
constexpr OPCodeTemplate LDD_Ea_deref_Ab_off10sx{(0x25_U32 << 22_U32) | 0x09_U32};
/// Load doubleword (Post-increment addressing mode) instruction
constexpr OPCodeTemplate LDD_Ea_deref_Ab_off10sx_postinc{(0x05_U32 << 22_U32) | 0x09_U32};
/// Load doubleword (Pre-increment addressing mode) instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDD_Ea_deref_Ab_off10sx_preinc{(0x15_U32 << 22_U32) | 0x09_U32};
/// Load address instruction
constexpr OPCodeTemplate LDA_Aa_deref_Ab_off16sx{0x00000099U};
/// Load address instruction (16b instruction)
constexpr OPCodeTemplate LDA_Ac_deref_Ab{0x00D4U};
/// Load doubleword to address instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LDDA_Pa_deref_Ab_off10sx{(0x27_U32 << 22_U32) | 0x09_U32};

/// Store byte instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STB_deref_Ab_off16sx_Da{0x000000E9U};
/// Store byte (Pre-increment addressing mode) instruction
constexpr OPCodeTemplate STB_deref_Ab_off10sx_Da_preinc{(0x10_U32 << 22_U32) | 0x89_U32};
/// Store byte (Post-increment addressing mode) instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STB_deref_Ab_off10sx_Da_postinc{(0x00_U32 << 22_U32) | 0x89_U32};
/// Store byte instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STB_deref_Ab_Da{0x0034U};
/// Store byte instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STB_deref_Ab_Da_postinc{0x0024U};
/// Store byte instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STB_deref_A15_off4zx_Da{0x0028U};
/// Store halfword instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STB_deref_Ab_off4srozx_D15{0x002CU};
/// Store halfword instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STH_deref_Ab_Da{0x00B4U};
/// Store halfword instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STH_deref_Ab_off16sx_Da{0x000000F9U};
/// Store halfword instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STH_deref_A15_off4zxls1_Da{0x00A8U};
/// Store halfword instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STH_deref_Ab_off4srozxls1_D15{0x00ACU};
/// Store word instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STW_deref_Ab_off16sx_Da{0x00000059U};
/// Store word instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STW_deref_A10_const8zxls2_D15{0x0078U};
/// Store word instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STW_deref_Ab_off4srozxls2_D15{0x006CU};
/// Store word instruction (16b instruction)
constexpr OPCodeTemplate STW_deref_Ab_Da{0x0074U};
/// Store word instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STW_deref_Ab_Da_postinc{0x0064U};
/// Store word instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STW_deref_A15_off4zxls2_Da{0x0068U};
/// Store word instruction (Pre-increment addressing mode)
constexpr OPCodeTemplate STW_deref_Ab_off10sx_Da_preinc{(0x14_U32 << 22_U32) | 0x89_U32};
/// Store doubleword instruction
constexpr OPCodeTemplate STD_deref_Ab_off10sx_Ea{0x09400089U};
/// Store doubleword (Pre-increment addressing mode) instruction
constexpr OPCodeTemplate STD_deref_Ab_off10sx_Ea_preinc{(0x15_U32 << 22_U32) | 0x89_U32};
/// Store doubleword (Post-increment addressing mode) instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STD_deref_Ab_off10sx_Ea_postinc{(0x05_U32 << 22_U32) | 0x89_U32};

/// Store address instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STA_deref_Ab_off16sx_Aa{0x000000B5U};
/// Store address instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STA_deref_Ab_Aa{0x00F4U};
/// Store doubleword address instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate STDA_deref_Ab_off10sx_Pa{(0x27_U32 << 22_U32) | 0x89_U32};
/// Store doubleword address instruction
constexpr OPCodeTemplate STDA_deref_Ab_off10sx_Pa_postinc{(0x07_U32 << 22_U32) | 0x89_U32};

/// Shift by signed constant instruction
constexpr OPCodeTemplate SH_Dc_Da_const9sx{0x0000008FU};
/// Shift by signed constant instruction, 16 bit
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate SH_Da_const4sx{0x0006U};
/// Shift by signed register instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate SH_Dc_Da_Db{0x0000000FU};
/// Shift arithmetic by signed constant instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate SHA_Dc_Da_const9sx{0x0020008FU};
/// Shift arithmetic by signed register instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate SHA_Dc_Da_Db{(0x01_U32 << 20_U32) | 0x0F_U32};

/// Subtract address instruction
constexpr OPCodeTemplate SUBA_Ac_Aa_Ab{0x00200001U};
/// Subtract address instruction (16 bit)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate SUBA_A10_const8zx{0x0020U};

/// Move constant to register instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOV_Dc_const16sx{0x0000003BU};
/// Move constant to register instruction (16b instruction)
constexpr OPCodeTemplate MOV_Da_const4sx{0x0082U};
/// Move constant to register instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOV_D15_const8zx{0x00DAU};
/// Move constant to extended register instruction
constexpr OPCodeTemplate MOV_Ec_const16sx{0x000000FBU};
/// Move register to register instruction
constexpr OPCodeTemplate MOV_Dc_Db{(0x1F_U32 << 20_U32) | 0x0B_U32};
/// Move register to register instruction (16b instruction)
constexpr OPCodeTemplate MOV_Da_Db{0x0002U};
/// Move register to extended register instruction with sign extension
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOV_Ec_Db{(0x80_U32 << 20_U32) | 0x0B_U32};
/// Move address register to register instruction
constexpr OPCodeTemplate MOVD_Dc_Ab{(0x4C_U32 << 20_U32) | 0x01_U32};
/// Move address register to register instruction (16b instruction)
constexpr OPCodeTemplate MOVD_Da_Ab{0x0080U};
/// Move unsigned constant to register instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOVU_Dc_const16zx{0x000000BBU};
/// Move constant to high register instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOVH_Dc_const16{0x0000007BU};

/// Move two registers to extended register instruction
constexpr OPCodeTemplate MOV_Ec_Da_Db{(0x81_U32 << 20_U32) | 0x0B_U32};

/// Move register to address register instruction
constexpr OPCodeTemplate MOVA_Ac_Db{(0x63_U32 << 20_U32) | 0x01_U32};
/// Move register to address register instruction (16b instruction)
constexpr OPCodeTemplate MOVA_Aa_Db{0x0060U};

/// Move unsigned constant to address register instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOVA_Aa_const4zx{0x00A0U}; // 16b instruction
/// Move address register to address register instruction (16b instruction)
constexpr OPCodeTemplate MOVAA_Aa_Ab{0x0040U};
/// Move unsigned constant to high address register instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOVHA_Ac_const16{0x00000091U};

/// Move the maximum value of Da and Db to Dc
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MAXU_Dc_Da_Db{0x01B0000BU};

/// Add register to register instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate ADD_Dc_Da_Db{0x0000000BU};
/// Add register to register instruction (16b instruction)
constexpr OPCodeTemplate ADD_Da_Db{0x0042U};
/// Add 4-bit constant to register instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate ADD_Da_const4sx{0x00C2U};
/// Add 4-bit constant to register instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate ADD_Da_D15_const4sx{0x0092U};
/// Add 4-bit constant to register instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate ADD_D15_Da_const4sx{0x009AU};
/// Subtract register from register instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate SUB_Dc_Da_Db{(0x08_U32 << 20_U32) | 0x0B_U32};
/// Subtract register from register instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate SUB_Da_Db{0x00A2U};

/// Add address register to address register instruction
constexpr OPCodeTemplate ADDA_Ac_Aa_Ab{(0x01_U32 << 20_U32) | 0x01_U32};
/// Add address register to address register instruction (16b instruction)
constexpr OPCodeTemplate ADDA_Aa_Ab{0x0030U};
/// Add address register to address register instruction (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate ADDA_Aa_const4sx{0x00B0U};
/// Add scaled register to address register instruction
constexpr OPCodeTemplate ADDSCA_Ac_Ab_Da_nSc{(0x60_U32 << 20_U32) | 0x01_U32};

/// Add constant to register extended (sets PSW carry bit) instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate ADDX_Dc_Da_const9sx{(0x04_U32 << 21_U32) | 0x8B_U32};
/// Add register to register extended (sets PSW carry bit) instruction
constexpr OPCodeTemplate ADDX_Dc_Da_Db{(0x04_U32 << 20_U32) | 0x0B_U32};

/// Add constant to register with carry (from PSW carry bit) instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate ADDC_Dc_Da_const9sx{(0x05_U32 << 21_U32) | 0x8B_U32};
/// Add register to register with carry (from PSW carry bit) instruction
constexpr OPCodeTemplate ADDC_Dc_Da_Db{(0x05_U32 << 20_U32) | 0x0B_U32};

/// Subtract register from register extended (sets PSW carry bit) instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate SUBX_Dc_Da_Db{(0x0C_U32 << 20_U32) | 0x0B_U32};
/// Subtract register from register with carry (from PSW carry bit) instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate SUBC_Dc_Da_Db{(0x0D_U32 << 20_U32) | 0x0B_U32};

/// Add immediate constant to register instruction
constexpr OPCodeTemplate ADDI_Dc_Da_const16sx{0x0000001BU};
/// Add immediate high constant to register instruction
constexpr OPCodeTemplate ADDIH_Dc_Da_const16{0x0000009BU};
/// Add immediate high constant to address register instruction
constexpr OPCodeTemplate ADDIHA_Ac_Aa_const16{0x00000011U};

/// (Reverse) Subtract register from constant
constexpr OPCodeTemplate RSUB_Dc_Da_const9sx{(0x08_U32 << 21_U32) | 0x8B_U32};
/// (Reverse) Subtract register from constant (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate RSUB_Da{0x5032U};

/// Multiply register with signed constant instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MUL_Dc_Da_const9sx{(0x01_U32 << 21_U32) | 0x53_U32};
/// Multiply register with register instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MUL_Dc_Da_Db{(0x0A_U32 << 16_U32) | 0x73_U32};
/// Multiply register with register instruction (16b instruction)
constexpr OPCodeTemplate MUL_Da_Db{0x00E2U};
/// Multiply register with signed constant (into extended register) instruction
constexpr OPCodeTemplate MUL_Ec_Da_const9sx{(0x03_U32 << 21_U32) | 0x53_U32};

/// Multiply register with unsigned constant (into extended register) instruction
constexpr OPCodeTemplate MULU_Ec_Da_const9zx{(0x02_U32 << 21_U32) | 0x53_U32};
/// Multiply register with register (into extended register) instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MULU_Ec_Da_Db{(0x68_U32 << 16_U32) | 0x73_U32};

/// Multiply-add register plus register times signed constant instruction
constexpr OPCodeTemplate MADD_Dc_Dd_Da_const9sx{(0x01_U32 << 21_U32) | 0x13_U32};
/// Multiply-add register plus register times register instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MADD_Dc_Dd_Da_Db{(0x0A_U32 << 16_U32) | 0x03_U32};

/// Move bit from register at position to register at position instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate INST_Dc_Da_pos1_Db_pos2{0x00000067U};
/// Move inverse bit from register at position to register at position instruction
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate INSNT_Dc_Da_pos1_Db_pos2{(0x01_U32 << 21_U32) | 0x67_U32};

#if TC_USE_HARD_F32_ARITHMETICS
// constexpr OPCodeTemplate ABSF_Dc_Da = 0x30U << 20U | 0x01_U32 << 16_U32 | 0x4BU;
// constexpr OPCodeTemplate NEGF_Dc_Da = 0x31U << 20U | 0x01_U32 << 16_U32 | 0x4BU;

/// Add float register to float register
constexpr OPCodeTemplate ADDF_Dc_Dd_Da = (0x02_U32 << 20_U32) | (0x01_U32 << 16_U32) | 0x6B_U32;
/// Subtract float register from float register
constexpr OPCodeTemplate SUBF_Dc_Dd_Da = (0x03_U32 << 20_U32) | 0x01_U32 << 16_U32 | 0x6B_U32;

/// Multiply float register with float register
constexpr OPCodeTemplate MULF_Dc_Da_Db = (0x4_U32 << 20_U32) | (0x01_U32 << 16_U32) | 0x4B_U32;
/// Divide float register by float register
constexpr OPCodeTemplate DIVF_Dc_Da_Db = (0x5_U32 << 20_U32) | (0x01_U32 << 16_U32) | 0x4B_U32;
#endif

/// Maximum of float registers
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MAXF_Dc_Da_Db{(0x32_U32 << 20_U32) | (0x01_U32 << 16_U32) | 0x4B_U32};
/// Minimum of float registers
constexpr OPCodeTemplate MINF_Dc_Da_Db{(0x33_U32 << 20_U32) | (0x01_U32 << 16_U32) | 0x4B_U32};

/// Compare float registers
constexpr OPCodeTemplate CMPF_Dc_Da_Db{(0x00_U32 << 20_U32) | (0x01_U32 << 16_U32) | 0x4B_U32};

#if TC_USE_HARD_F32_TO_I32_CONVERSIONS
/// Float register to signed integer, round towards zero
constexpr OPCodeTemplate FTOIZ_Dc_Da = (0x13_U32 << 20_U32) | (0x01_U32 << 16_U32) | 0x4B_U32;
/// Float register to unsigned integer, round towards zero
constexpr OPCodeTemplate FTOUZ_Dc_Da = (0x17_U32 << 20_U32) | (0x01_U32 << 16_U32) | 0x4B_U32;

/// Signed integer register to float
constexpr OPCodeTemplate ITOF_Dc_Da = (0x14_U32 << 20_U32) | (0x01_U32 << 16_U32) | 0x4B_U32;
/// Unsigned integer register to float
constexpr OPCodeTemplate UTOF_Dc_Da = (0x16_U32 << 20_U32) | (0x01_U32 << 16_U32) | 0x4B_U32;
#endif

// constexpr OPCodeTemplate ABSDF_Ec_Ea = 0x30U << 20U | (0x02_U32 << 16_U32) | 0x4BU;
// constexpr OPCodeTemplate NEGDF_Ec_Ea = 0x31U << 20U | (0x02_U32 << 16_U32) | 0x4BU;

/// Add double register to double register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate ADDDF_Ec_Ed_Ea{(0x02_U32 << 20_U32) | (0x02_U32 << 16_U32) | 0x6B_U32};
/// Subtract double register from double register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate SUBDF_Ec_Ed_Ea{(0x03_U32 << 20_U32) | (0x02_U32 << 16_U32) | 0x6B_U32};

/// Multiply double register with double register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MULDF_Ec_Ea_Eb{(0x4_U32 << 20_U32) | (0x02_U32 << 16_U32) | 0x4B_U32};
/// Divide double register by double register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate DIVDF_Ec_Ea_Eb{(0x5_U32 << 20_U32) | (0x02_U32 << 16_U32) | 0x4B_U32};
/// Maximum of double registers
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MAXDF_Ec_Ea_Eb{(0x32_U32 << 20_U32) | (0x02_U32 << 16_U32) | 0x4B_U32};
/// Minimum of double registers
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MINDF_Ec_Ea_Eb{(0x33_U32 << 20_U32) | (0x02_U32 << 16_U32) | 0x4B_U32};

/// Double register to signed integer, round towards zero
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate DFTOIZ_Dc_Ea{(0x13_U32 << 20_U32) | (0x02_U32 << 16_U32) | 0x4B_U32};
/// Double register to unsigned integer, round towards zero
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate DFTOUZ_Dc_Ea{(0x17_U32 << 20_U32) | (0x02_U32 << 16_U32) | 0x4B_U32};
/// Double register to signed long integer, round towards zero
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate DFTOLZ_Ec_Ea{(0x1B_U32 << 20_U32) | (0x02_U32 << 16_U32) | 0x4B_U32};
/// Double register to unsigned long integer, round towards zero
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate DFTOULZ_Ec_Ea{(0x1F_U32 << 20_U32) | (0x02_U32 << 16_U32) | 0x4B_U32};

/// Double register to float
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate DFTOF_Dc_Ea{(0x28_U32 << 20_U32) | (0x02_U32 << 16_U32) | 0x4B_U32};
/// Signed integer register to double
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate ITODF_Ec_Da{(0x14_U32 << 20_U32) | (0x02_U32 << 16_U32) | 0x4B_U32};
/// Unsigned integer register to double
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate UTODF_Ec_Da{(0x16_U32 << 20_U32) | (0x02_U32 << 16_U32) | 0x4B_U32};
/// Signed long integer register to double
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LTODF_Ec_Ea{(0x26_U32 << 20_U32) | (0x02_U32 << 16_U32) | 0x4B_U32};
/// Unsigned long integer register to double
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate ULTODF_Ec_Ea{(0x27_U32 << 20_U32) | (0x02_U32 << 16_U32) | 0x4B_U32};
/// Float register to double
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate FTODF_Ec_Da{(0x29_U32 << 20_U32) | (0x02_U32 << 16_U32) | 0x4B_U32};

/// Compare double registers
constexpr OPCodeTemplate CMPDF_Dc_Ea_Eb{(0x00_U32 << 20_U32) | (0x02_U32 << 16_U32) | 0x4B_U32};

#if TC_USE_DIV
/// Divide signed register by signed register into extended register (One register contains division result, other
/// modulo)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate DIV_Ec_Da_Db{(0x20_U32 << 20_U32) | (0x01_U32 << 16_U32) | 0x4B_U32};
/// Divide unsigned register by unsigned register into extended register (One register contains division result, other
/// modulo)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate DIVU_Ec_Da_Db{(0x21_U32 << 20_U32) | (0x01_U32 << 16_U32) | 0x4B_U32};
#else
/// Init step-wise signed division
constexpr OPCodeTemplate DVINIT_Ec_Da_Db = (0x1A_U32 << 20_U32) | 0x4B_U32;
/// Init step-wise unsigned division
constexpr OPCodeTemplate DVINITU_Ec_Da_Db = (0x0A_U32 << 20_U32) | 0x4B_U32;
/// Perform signed division step
constexpr OPCodeTemplate DVSTEP_Ec_Ed_Db = (0x0F_U32 << 20_U32) | 0x6B_U32;
/// Perform unsigned division step
constexpr OPCodeTemplate DVSTEPU_Ec_Ed_Db = (0x0E_U32 << 20_U32) | 0x6B_U32;
#endif

/// Divide signed long by signed long
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate DIV64_Ec_Ea_Eb{(0x20_U32 << 20_U32) | (0x2_U32 << 16_U32) | 0x4B_U32};
/// Divide unsigned long by unsigned long
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate DIV64U_Ec_Ea_Eb{(0x21_U32 << 20_U32) | (0x2_U32 << 16_U32) | 0x4B_U32};
/// Calculate modulo from signed long division
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate REM64_Ec_Ea_Eb{(0x34_U32 << 20_U32) | (0x2_U32 << 16_U32) | 0x4B_U32};
/// Calculate modulo from unsigned long division
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate REM64U_Ec_Ea_Eb{(0x35_U32 << 20_U32) | (0x2_U32 << 16_U32) | 0x4B_U32};

/// Bitwise AND of register with unsigned constant
constexpr OPCodeTemplate AND_Dc_Da_const9zx{(0x08_U32 << 21_U32) | 0x8F_U32};
/// Bitwise AND of register with register
constexpr OPCodeTemplate AND_Dc_Da_Db{(0x08_U32 << 20_U32) | 0x0F_U32};
/// Bitwise AND of register with register (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate AND_Da_Db{0x0026U};
/// Bitwise AND of D15 with unsigned constant (16b instruction)
constexpr OPCodeTemplate AND_D15_const8zx{0x0016U};

/// Bitwise OR of register with unsigned constant
constexpr OPCodeTemplate OR_Dc_Da_const9zx{(0x0A_U32 << 21_U32) | 0x8F_U32};
/// Bitwise OR of register with register
constexpr OPCodeTemplate OR_Dc_Da_Db{(0x0A_U32 << 20_U32) | 0x0F_U32};
/// Bitwise OR of register with register (16b instruction)
constexpr OPCodeTemplate OR_Da_Db{0x00A6U};
/// Bitwise OR of D15 with unsigned constant (16b instruction)
constexpr OPCodeTemplate OR_D15_const8zx{0x0096U};

/// Bitwise XOR of register with unsigned constant
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate XOR_Dc_Da_const9zx{(0x0C_U32 << 21_U32) | 0x8F_U32};
/// Bitwise XOR of register with register
constexpr OPCodeTemplate XOR_Dc_Da_Db{(0x0C_U32 << 20_U32) | 0x0F_U32};
/// Bitwise XOR of register with register (16b instruction)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate XOR_Da_Db{0x00C6U};

/// Bit reflect the entire word (special case for SHUFFLE)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate BIT_REFLECT_Dc_Da{(0x07_U32 << 21_U32) | (0x11B_U32 << 12_U32) | 0x8F_U32};
/// Copy the least significant input byte into all four byte positions (special case for SHUFFLE)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate COPY_BYTE_TO_ALL_Dc_Da{(0x07_U32 << 21_U32) | (0x000_U32 << 12_U32) | 0x8F_U32};

/// Count leading zeros of register
constexpr OPCodeTemplate CLZ_Dc_Da{(0x1B_U32 << 20_U32) | 0x0F_U32};
/// Count population (ones) in register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate POPCNTW_Dc_Da{(0x22_U32 << 20_U32) | 0x4B_U32};

/// Test if register equal to signed constant
constexpr OPCodeTemplate EQ_Dc_Da_const9sx{(0x10_U32 << 21_U32) | 0x8B_U32};
/// Test if register equal to signed constant, save result in D15. 16 bit instruction
constexpr OPCodeTemplate EQ_D15_Da_const4sx{0x00BA_U32};
/// Test if register equal to register
constexpr OPCodeTemplate EQ_Dc_Da_Db{(0x10_U32 << 20_U32) | 0x0B_U32};
/// Test if register equal to register, save result in D15. 16 bit instruction
constexpr OPCodeTemplate EQ_D15_Da_Db{0x3A_U32};
/// Test if register not equal to signed constant
constexpr OPCodeTemplate NE_Dc_Da_const9sx{(0x11_U32 << 21_U32) | 0x8B_U32};
/// Test if register not equal to register
constexpr OPCodeTemplate NE_Dc_Da_Db{(0x11_U32 << 20_U32) | 0x0B_U32};
/// Test if register less than signed constant
constexpr OPCodeTemplate LT_Dc_Da_const9sx{(0x12_U32 << 21_U32) | 0x8B_U32};
/// Test if register less than signed constant, save result in d15, 16bit instruction
constexpr OPCodeTemplate LT_D15_Da_const4sx{0xFA_U32};
/// Test if register less than signed register
constexpr OPCodeTemplate LT_Dc_Da_Db{(0x12_U32 << 20_U32) | 0x0B_U32};
/// Test if register less than signed register, save result in d15, 16bit instruction
constexpr OPCodeTemplate LT_D15_Da_Db{0x7A_U32};
/// Test if register less than unsigned constant
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LTU_Dc_Da_const9zx{(0x13_U32 << 21_U32) | 0x8B_U32};
/// Test if register less than unsigned register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate LTU_Dc_Da_Db{(0x13_U32 << 20_U32) | 0x0B_U32};
/// Test if register greater or equal than signed constant
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate GE_Dc_Da_const9sx{(0x14_U32 << 21_U32) | 0x8B_U32};
/// Test if register greater or equal than signed register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate GE_Dc_Da_Db{(0x14_U32 << 20_U32) | 0x0B_U32};
/// Test if register greater or equal than unsigned constant
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate GEU_Dc_Da_const9zx{(0x15_U32 << 21_U32) | 0x8B_U32};
/// Test if register greater or equal than unsigned register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate GEU_Dc_Da_Db{(0x15_U32 << 20_U32) | 0x0B_U32};

/// Test if address register equal to address register
constexpr OPCodeTemplate EQA_Dc_Aa_Ab{(0x40_U32 << 20_U32) | 0x01_U32};
/// Test if address register not equal to address register
constexpr OPCodeTemplate NEA_Dc_Aa_Ab{(0x41_U32 << 20_U32) | 0x01_U32};
/// Test if address register less than address register (unsigned)
constexpr OPCodeTemplate LTA_Dc_Aa_Ab{(0x42_U32 << 20_U32) | 0x01_U32};
/// Test if address register greater or equal to address register (unsigned)
constexpr OPCodeTemplate GEA_Dc_Aa_Ab{(0x43_U32 << 20_U32) | 0x01_U32};

/// Equal, AND Accumulating with signed constant
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate ANDEQ_Dc_Da_const9sx{(0x20_U32 << 21_U32) | 0x8B_U32};
/// Equal, AND Accumulating with register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate ANDEQ_Dc_Da_Db{(0x20_U32 << 20_U32) | 0x0B_U32};
/// Less than, AND Accumulating with unsigned constant
constexpr OPCodeTemplate ANDLTU_Dc_Da_const9zx{(0x23_U32 << 21_U32) | 0x8B_U32};
/// Less than, AND Accumulating with register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate ANDLTU_Dc_Da_Db{(0x23_U32 << 20_U32) | 0x0B_U32};
/// Greater equal to, AND Accumulating with unsigned constant
constexpr OPCodeTemplate ANDGEU_Dc_Da_const9zx{(0x25_U32 << 21_U32) | 0x8B_U32};
/// Greater equal to, AND Accumulating with register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate ANDGEU_Dc_Da_Db{(0x25_U32 << 20_U32) | 0x0B_U32};
/// Not equal to, OR Accumulating with signed constant
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate ORNE_Dc_Da_const9sx{(0x28_U32 << 21_U32) | 0x8B_U32};
/// Not equal to, OR Accumulating with register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate ORNE_Dc_Da_Db{(0x28_U32 << 20_U32) | 0x0B_U32};
/// Less than, OR Accumulating with signed constant
constexpr OPCodeTemplate ORLT_Dc_Da_const9sx{(0x29_U32 << 21_U32) | 0x8B_U32};
/// Less than, OR Accumulating with register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate ORLT_Dc_Da_Db{(0x29_U32 << 20_U32) | 0x0B_U32};
/// Less than, OR Accumulating with unsigned constant
constexpr OPCodeTemplate ORLTU_Dc_Da_const9zx{(0x2A_U32 << 21_U32) | 0x8B_U32};
/// Less than, OR Accumulating with register
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate ORLTU_Dc_Da_Db{(0x2A_U32 << 20_U32) | 0x0B_U32};

/// Load lower context from address
constexpr OPCodeTemplate LDLCX_Ab_off10sx{(0x24_U32 << 22_U32) | 0x49_U32};
/// Load upper context from address
constexpr OPCodeTemplate LDUCX_Ab_off10sx{(0x25_U32 << 22_U32) | 0x49_U32};
/// Store lower context from address
constexpr OPCodeTemplate STLCX_Ab_off10sx{(0x26_U32 << 22_U32) | 0x49_U32};
/// Store upper context from address
constexpr OPCodeTemplate STUCX_Ab_off10sx{(0x27_U32 << 22_U32) | 0x49_U32};

/// Move from core register (offset) to register
constexpr OPCodeTemplate MFCR_Dc_const16{0x4DU};
/// Restore lower context
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate RSLCX{(0x09_U32 << 22_U32) | 0x0D_U32};

/// If the contents of data register D[d] are non-zero, copy the contents of data register D[a] to data register D[c];
/// otherwise copy the contents of either D[b] to D[c].
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate SEL_Dc_Da_Db_Dd{(0x04_U32 << 20_U32) | 0x2B_U32};

/// If the contents of data register D[d] are non-zero, copy the contents of data register D[a] to data register D[c];
/// otherwise copy const9 to D[c].
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate SEL_Dc_Da_Dd_const9sx{(0x04_U32 << 21_U32) | 0xAB_U32};

/// If the contents of data register D[d] are zero, copy the contents of data register D[a] to data register D[c];
/// otherwise copy const9 to D[c].
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate SELN_Dc_Da_Dd_const9sx{(0x05_U32 << 21_U32) | 0xAB_U32};

} // namespace tc
} // namespace vb

#endif

#endif

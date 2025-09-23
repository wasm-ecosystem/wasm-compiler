///
/// @file x86_64_encoding.hpp
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
#ifndef X86_64_ENCODING_HPP
#define X86_64_ENCODING_HPP

#include <cstdint>

#include "src/core/common/util.hpp"
#include "src/core/compiler/common/BranchCondition.hpp"

namespace vb {
namespace x86_64 {

///
/// @brief Native registers and their encoding that can be placed into the respective fields in an instruction
/// NOTE: REG::NONE will be used to represent an invalid register (or no register at all)
///
// coverity[autosar_cpp14_a7_2_4_violation] autosar required style is more bug prone
enum class REG : uint32_t { // clang-format off
	A, C, D, B, SP, BP, SI, DI, R8, R9, R10, R11, R12, R13, R14, R15,
	XMM0 = 0b0001'0000, XMM1, XMM2, XMM3, XMM4, XMM5, XMM6, XMM7, XMM8, XMM9, XMM10, XMM11, XMM12, XMM13, XMM14, XMM15,
  NUMREGS,
	NONE = 0b1000'0000
}; // clang-format on

constexpr uint32_t totalNumRegs{static_cast<uint32_t>(REG::NUMREGS)}; ///< Total number of registers in the enum

///
/// @brief Left shift operator to be able to easily create RegMasks
///
/// Left shifts an unsigned integer by the underlying register representation
///
/// @param lhs Left hand side integer
/// @param rhs Right hand side register
/// @return uint32_t Resultant integer
inline uint32_t operator<<(uint32_t const lhs, REG const rhs) VB_NOEXCEPT {
  return static_cast<uint32_t>(lhs << static_cast<uint32_t>(rhs));
}

///
/// @brief AND operator for registers
///
/// To check whether a register is an FPR or GPR
///
/// @param lhs Left hand side register
/// @param rhs Right hand side integer
/// @return uint8_t Resultant integer
inline uint8_t operator&(REG const lhs, uint8_t const rhs) VB_NOEXCEPT {
  return static_cast<uint8_t>(lhs) & rhs;
}

namespace RegUtil {

///
/// @brief Checks whether a register is a general purpose register (as opposed to a floating point register)
///
/// @param reg Register to check
/// @return bool Whether the register is a general purpose register
inline bool isGPR(REG const reg) VB_NOEXCEPT {
  return (static_cast<uint32_t>(reg) & 0b1'0000U) == 0U;
}

} // namespace RegUtil

///
/// @brief x86_64 CPU condition codes
///
/// @note
///    Condition   |  Meaning                     |  Flags
///      O         |  overflow                    |  OF
///      NO        |  no overflow                 |  ~OF
///      B         |  below (unsigned)            |  CF
///      AE        |  above or equal (unsigned)   |  ~CF
///      E         |  equal/zero                  |  ZF
///      NE        |  not equal/zero              |  ~ZF
///      BE        |  below or equal (unsigned)   |  CF | ZF
///      A         |  above (unsigned)            |  ~CF & ~ZF
///      S         |  negative                    |  SF
///      NS        |  non negative                |  ~SF
///      P         |  parity                      |  PF
///      NP        |  no parity                   |  ~PF
///      L         |  less (signed)               |  SF ^ 0F
///      GE        |  greater than (signed)       |  ~(SF ^ 0F)
///      LE        |  less or equal (signed)      |  (SF ^ OF) | ZF
///      G         |  greater (signed)            |  ~(SF ^ 0F) & ~ZF
///      C         |  carry                       |  CF
///      NC        |  no carry                    |  ~CF
///
// coverity[autosar_cpp14_a7_2_4_violation] autosar required style is more bug prone
enum class CC : uint8_t { O, NO, B, AE, E, NE, BE, A, S, NS, P, NP, L, GE, LE, G, C = B, NC = AE, NONE = 0xFF };

///
/// @brief Invert the condition code (i.e. return CC::LT from CC:GE)
///
/// @param cc Condition code to invert
/// @return CC Inverted condition code
inline CC negateCC(CC const cc) VB_NOEXCEPT {
  // coverity[autosar_cpp14_a7_2_1_violation]
  return static_cast<CC>(static_cast<uint8_t>(cc) ^ 0b1U);
}

///
/// @brief Plus operator so native opcodes can easily add a condition code
///
/// @param lhs Base uint8_t, often representing part of a native opcode
/// @param rhs Condition code to add to the base
/// @return uint8_t Resulting encoding
inline uint8_t operator+(uint8_t const lhs, CC const rhs) VB_NOEXCEPT {
  return static_cast<uint8_t>(lhs + static_cast<uint8_t>(rhs));
}

///
/// @brief Find the corresponding CPU condition code to an abstract branch condition
///
/// @param branchCond Input branch condition
/// @return CC Corresponding CPU condition code
inline CC CCforBC(BC const branchCond) VB_NOEXCEPT {
  assert(static_cast<uint8_t>(branchCond) <= static_cast<uint8_t>(BC::UNCONDITIONAL) && "Invalid branch condition");
  // coverity[autosar_cpp14_a8_5_2_violation]
  constexpr auto CCforBC = make_array(CC::NE, CC::E, CC::E, CC::NE, CC::L, CC::B, CC::G, CC::A, CC::LE, CC::BE, CC::GE, CC::AE, CC::E, CC::NE, CC::B,
                                      CC::A, CC::BE, CC::AE, CC::NONE);
  return CCforBC[static_cast<uint8_t>(branchCond)];
}

///
/// @brief Abstract definition for the input argument of an abstract instruction
///
/// This defines the input type (I32, I64, F32, F64) and whether this instruction can handle floating point, general
/// purpose registers, memory locations or an immediate of a certain encoding. Only the encodings used in selectInstr
/// are defined here and which are thus selected from an array of instructions ArgType::TYPEMASK can be used to extract
/// the underlying input type (I32 etc.) for an ArgType
///
// coverity[autosar_cpp14_a7_2_4_violation] autosar required style is more bug prone
enum class ArgType : uint8_t { // clang-format off
	NONE = 0b00000000,
	I32  = 0b00100000, r32,  rm32,      imm32, imm8sx_32, imm8_32, c1_32,
	I64  = 0b01000000, r64,  rm64, imm32sx_64, imm8sx_64, imm8_64, c1_64,
	F32  = 0b01100000, r32f, rm32f, rm32f_128_restrictm,
	F64  = 0b10000000, r64f, rm64f, rm64f_128_restrictm,
	TYPEMASK = 0b11100000
}; // clang-format on

///
/// @brief REX flag
///
class REX {
public:
  static constexpr uint8_t NONE{0x0U};        ///< No REX prefix
  static constexpr uint8_t BASE{0b01000000U}; ///< REX base prefix
  static constexpr uint8_t W{0b01001000U};    ///< REX.W: 64-bit operand size
  static constexpr uint8_t R{0b01000100U};    ///< REX.R: ModR/M reg field extension
  static constexpr uint8_t X{0b01000010U};    ///< REX.X: SIB index field extension
  static constexpr uint8_t B{0b01000001U};    ///< REX.B: ModR/M r/m field, SIB base field extension
};

///
/// @brief OPCode extension (e.g. /1, /2, /r or +r)
/// Note: We also encode +r (register encoding in/add to opcode, e.g. push/pop or mov r64, imm64) in this flag to save
/// memory as RADD.
///
// coverity[autosar_cpp14_a7_2_4_violation] autosar required style is more bug prone
enum class OPCodeExt : uint8_t { i0, i1, i2, i3, i4, i5, i6, i7, RADD = 0xFD, R = 0xFE, NONE = 0xFF };

///
/// @brief Indicates whether the R or RM argument of an instruction is an 8-bit type
///
enum class B8F : uint8_t { NONE = 0b00, R = 0b01, RM = 0b10 };

///
/// @brief Binary OR operator for the B8F flag
///
/// @param lhs Left hand side
/// @param rhs Right hand side
/// @return B8F Resulting B8F flag
inline constexpr B8F operator|(B8F const lhs, B8F const rhs) VB_NOEXCEPT {
  // coverity[autosar_cpp14_a7_2_1_violation]
  return static_cast<B8F>(static_cast<uint8_t>(lhs) | static_cast<uint8_t>(rhs));
}

///
/// @brief Basic template for x86_64 OPCodes
///
struct OPCodeTemplate final {
  uint8_t prefix; ///< OPCode prefix
  uint8_t rex;    ///< OPCode REX flag

  ///
  /// @brief Whether the R or RM argument/input to the instruction is an 8-bit argument
  ///
  /// In the case of registers, the assembler needs to know this to emit an extra REX::BASE for SIL, DIL, SPL,  BPL
  /// (otherwise those are ah, bh etc.)
  ///
  B8F b8Flag;
  OPCodeExt extension; ///< OPCode extension
  uint32_t opcode;     ///< Basic opcode
};

///
/// @brief Complete description of an x86_64 instruction
///
/// This includes an opcode template, the destination and source types, whether the sources are commutative and whether
/// this instruction represents a unary instruction
///
struct AbstrInstr final {
  OPCodeTemplate opTemplate; ///< Basic opcode template
  ArgType dstType;           ///< Destination type
  ArgType srcType;           ///< Source type
  bool unop;                 ///< Whether this is a unary operation. True If the number of input register is 1(destination register
                             ///< doesn't affect the output).
  bool commutative;          ///< Whether the inputs (destination and source) are commutative
};

/// @brief RDTSC: Read Time-Stamp Counter to RAX(low 32 bits), RDX(high 32 bits)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr RDTSC{{0U, REX::NONE, B8F::NONE, OPCodeExt::NONE, 0x0F31U}, ArgType::NONE, ArgType::NONE, false, false};

/// @brief UCOMISS xmm1, xmm2/m32: Compare low single-precision floating-point values in xmm1 and xmm2/mem32 and set the
/// EFLAGS flags accordingly.
constexpr AbstrInstr UCOMISS_rf_rmf{{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F2EU}, ArgType::r32f, ArgType::rm32f, false, false};
/// @brief UCOMISD xmm1, xmm2/m64: Compare low double-precision floating-point values in xmm1 and xmm2/mem64 and set the
/// EFLAGS flags accordingly.
constexpr AbstrInstr UCOMISD_rf_rmf{{0x66U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F2EU}, ArgType::r64f, ArgType::rm64f, false, false};

/// @brief LZCNT r32, r/m32: Count the number of leading zero bits in r/m32, return result in r32.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr LZCNT_r32_rm32{{0xF3U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0FBDU}, ArgType::r32, ArgType::rm32, true, false};
/// @brief LZCNT r64, r/m64: Count the number of leading zero bits in r/m64, return result in r64.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr LZCNT_r64_rm64{{0xF3U, REX::W, B8F::NONE, OPCodeExt::R, 0x0FBDU}, ArgType::r64, ArgType::rm64, true, false};
/// @brief TZCNT r32, r/m32: Count the number of trailing zero bits in r/m32, return result in r32.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr TZCNT_r32_rm32{{0xF3U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0FBCU}, ArgType::r32, ArgType::rm32, true, false};
/// @brief TZCNT r64, r/m64: Count the number of trailing zero bits in r/m64, return result in r64.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr TZCNT_r64_rm64{{0xF3U, REX::W, B8F::NONE, OPCodeExt::R, 0x0FBCU}, ArgType::r64, ArgType::rm64, true, false};
/// @brief POPCNT r32, r/m32: POPCNT on r/m32
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr POPCNT_r32_rm32{{0xF3U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0FB8U}, ArgType::r32, ArgType::rm32, true, false};
/// @brief POPCNT r64, r/m64: POPCNT on r/m64
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr POPCNT_r64_rm64{{0xF3U, REX::W, B8F::NONE, OPCodeExt::R, 0x0FB8U}, ArgType::r64, ArgType::rm64, true, false};

/// @brief CMP r/m32, imm8: Compare imm8 with r/m32.
constexpr AbstrInstr CMP_rm32_imm8sx{{0U, REX::NONE, B8F::NONE, OPCodeExt::i7, 0x83U}, ArgType::rm32, ArgType::imm8sx_32, false, false};
/// @brief CMP r/m64, imm8: Compare imm8 with r/m64.
constexpr AbstrInstr CMP_rm64_imm8sx{{0U, REX::W, B8F::NONE, OPCodeExt::i7, 0x83U}, ArgType::rm64, ArgType::imm8sx_64, false, false};
/// @brief CMP r/m32, imm32: Compare imm32 with r/m32.
constexpr AbstrInstr CMP_rm32_imm32{{0U, REX::NONE, B8F::NONE, OPCodeExt::i7, 0x81U}, ArgType::rm32, ArgType::imm32, false, false};
/// @brief CMP r/m64, imm32: Compare imm32 sign-extended to 64-bits with r/m64.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr CMP_rm64_imm32sx{{0U, REX::W, B8F::NONE, OPCodeExt::i7, 0x81U}, ArgType::rm64, ArgType::imm32sx_64, false, false};
/// @brief CMP r/m32, r32: Compare r32 with r/m32.
constexpr AbstrInstr CMP_rm32_r32{{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x39U}, ArgType::rm32, ArgType::r32, false, false};
/// @brief CMP r/m64,r64: Compare r64 with r/m64.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr CMP_rm64_r64{{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x39U}, ArgType::rm64, ArgType::r64, false, false};
/// @brief CMP r32, r/m32: Compare r/m32 with r32.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr CMP_r32_rm32{{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x3BU}, ArgType::r32, ArgType::rm32, false, false};
/// @brief CMP r64, r/m64: Compare r/m64 with r64.
constexpr AbstrInstr CMP_r64_rm64{{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x3BU}, ArgType::r64, ArgType::rm64, false, false};

/// @brief ADD r/m32, imm8: Add sign-extended imm8 to r/m32.
constexpr AbstrInstr ADD_rm32_imm8sx{{0U, REX::NONE, B8F::NONE, OPCodeExt::i0, 0x83U}, ArgType::rm32, ArgType::imm8sx_32, false, true};
/// @brief ADD r/m64, imm8: Add sign-extended imm8 to r/m64.
constexpr AbstrInstr ADD_rm64_imm8sx{{0U, REX::W, B8F::NONE, OPCodeExt::i0, 0x83U}, ArgType::rm64, ArgType::imm8sx_64, false, true};
/// @brief ADD r/m32, imm32: Add imm32 to r/m32.
constexpr AbstrInstr ADD_rm32_imm32{{0U, REX::NONE, B8F::NONE, OPCodeExt::i0, 0x81U}, ArgType::rm32, ArgType::imm32, false, true};
/// @brief ADD r/m64, imm32: Add imm32 sign-extended to 64-bits to r/m64.
constexpr AbstrInstr ADD_rm64_imm32sx{{0U, REX::W, B8F::NONE, OPCodeExt::i0, 0x81U}, ArgType::rm64, ArgType::imm32sx_64, false, true};
/// @brief ADD r/m32, r32: Add r32 to r/m32.
constexpr AbstrInstr ADD_rm32_r32{{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x01U}, ArgType::rm32, ArgType::r32, false, true};
/// @brief ADD r/m64, r64: Add r64 to r/m64.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ADD_rm64_r64{{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x01U}, ArgType::rm64, ArgType::r64, false, true};
/// @brief ADD r32, r/m32: Add r/m32 to r32.
constexpr AbstrInstr ADD_r32_rm32{{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x03U}, ArgType::r32, ArgType::rm32, false, true};
/// @brief ADD r64, r/m64: Add r/m64 to r64.
constexpr AbstrInstr ADD_r64_rm64{{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x03U}, ArgType::r64, ArgType::rm64, false, true};

/// @brief SUB r/m32, imm8: Subtract sign-extended imm8 from r/m32.
constexpr AbstrInstr SUB_rm32_imm8sx{{0U, REX::NONE, B8F::NONE, OPCodeExt::i5, 0x83U}, ArgType::rm32, ArgType::imm8sx_32, false, false};
/// @brief SUB r/m64, imm8: Subtract sign-extended imm8 from r/m64.
constexpr AbstrInstr SUB_rm64_imm8sx{{0U, REX::W, B8F::NONE, OPCodeExt::i5, 0x83U}, ArgType::rm64, ArgType::imm8sx_64, false, false};
/// @brief SUB r/m32, imm32: Subtract imm32 from r/m32.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SUB_rm32_imm32{{0U, REX::NONE, B8F::NONE, OPCodeExt::i5, 0x81U}, ArgType::rm32, ArgType::imm32, false, false};
/// @brief SUB r/m64, imm32: Subtract imm32 sign-extended to 64-bits from r/m64.
constexpr AbstrInstr SUB_rm64_imm32sx{{0U, REX::W, B8F::NONE, OPCodeExt::i5, 0x81U}, ArgType::rm64, ArgType::imm32sx_64, false, false};
/// @brief SUB r/m32, r32: Subtract r32 from r/m32.
constexpr AbstrInstr SUB_rm32_r32{{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x29U}, ArgType::rm32, ArgType::r32, false, false};
/// @brief SUB r/m64, r64: Subtract r64 from r/m64.
constexpr AbstrInstr SUB_rm64_r64{{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x29U}, ArgType::rm64, ArgType::r64, false, false};
/// @brief SUB r32, r/m32: Subtract r/m32 from r32.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SUB_r32_rm32{{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x2BU}, ArgType::r32, ArgType::rm32, false, false};
/// @brief SUB r64, r/m64: Subtract r/m64 from r64.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SUB_r64_rm64{{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x2BU}, ArgType::r64, ArgType::rm64, false, false};

/// @brief AND r/m32, imm8: r/m32 AND imm8 (sign-extended).
constexpr AbstrInstr AND_rm32_imm8sx{{0U, REX::NONE, B8F::NONE, OPCodeExt::i4, 0x83U}, ArgType::rm32, ArgType::imm8sx_32, false, true};
/// @brief AND r/m64, imm8: r/m64 AND imm8 (sign-extended).
constexpr AbstrInstr AND_rm64_imm8sx{{0U, REX::W, B8F::NONE, OPCodeExt::i4, 0x83U}, ArgType::rm64, ArgType::imm8sx_64, false, true};
/// @brief AND r/m32, imm32: r/m32 AND imm32.
constexpr AbstrInstr AND_rm32_imm32{{0U, REX::NONE, B8F::NONE, OPCodeExt::i4, 0x81U}, ArgType::rm32, ArgType::imm32, false, true};
/// @brief AND r/m64, imm32: r/m64 AND imm32 sign extended to 64-bits.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr AND_rm64_imm32sx{{0U, REX::W, B8F::NONE, OPCodeExt::i4, 0x81U}, ArgType::rm64, ArgType::imm32sx_64, false, true};
/// @brief AND r/m32, r32: r/m32 AND r32.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr AND_rm32_r32{{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x21U}, ArgType::rm32, ArgType::r32, false, true};
/// @brief AND r/m64, r64: r/m64 AND r32.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr AND_rm64_r64{{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x21U}, ArgType::rm64, ArgType::r64, false, true};
/// @brief AND r32, r/m32: r32 AND r/m32.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr AND_r32_rm32{{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x23U}, ArgType::r32, ArgType::rm32, false, true};
/// @brief AND r64, r/m64: r64 AND r/m64.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr AND_r64_rm64{{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x23U}, ArgType::r64, ArgType::rm64, false, true};

/// @brief OR r/m32, imm8: r/m32 OR imm8 (sign-extended).
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr OR_rm32_imm8sx{{0U, REX::NONE, B8F::NONE, OPCodeExt::i1, 0x83U}, ArgType::rm32, ArgType::imm8sx_32, false, true};
/// @brief OR r/m64, imm8: r/m64 OR imm8 (sign-extended).
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr OR_rm64_imm8sx{{0U, REX::W, B8F::NONE, OPCodeExt::i1, 0x83U}, ArgType::rm64, ArgType::imm8sx_64, false, true};
/// @brief OR r/m32, imm32: r/m32 OR imm32.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr OR_rm32_imm32{{0U, REX::NONE, B8F::NONE, OPCodeExt::i1, 0x81U}, ArgType::rm32, ArgType::imm32, false, true};
/// @brief OR r/m64, imm32: r/m64 OR imm32 (sign-extended).
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr OR_rm64_imm32sx{{0U, REX::W, B8F::NONE, OPCodeExt::i1, 0x81U}, ArgType::rm64, ArgType::imm32sx_64, false, true};
/// @brief OR r/m32, r32: r/m32 OR r32.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr OR_rm32_r32{{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x09U}, ArgType::rm32, ArgType::r32, false, true};
/// @brief OR r/m64, r64: r/m64 OR r64.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr OR_rm64_r64{{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x09U}, ArgType::rm64, ArgType::r64, false, true};
/// @brief OR r32, r/m32: r32 OR r/m32.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr OR_r32_rm32{{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0BU}, ArgType::r32, ArgType::rm32, false, true};
/// @brief OR r64, r/m64: r64 OR r/m64.
constexpr AbstrInstr OR_r64_rm64{{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x0BU}, ArgType::r64, ArgType::rm64, false, true};

/// @brief XOR r/m32, imm8: r/m32 XOR imm8 (sign-extended).
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr XOR_rm32_imm8sx{{0U, REX::NONE, B8F::NONE, OPCodeExt::i6, 0x83U}, ArgType::rm32, ArgType::imm8sx_32, false, true};
/// @brief XOR r/m64, imm8: r/m64 XOR imm8 (sign-extended).
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr XOR_rm64_imm8sx{{0U, REX::W, B8F::NONE, OPCodeExt::i6, 0x83U}, ArgType::rm64, ArgType::imm8sx_64, false, true};
/// @brief XOR r/m32, imm32: r/m32 XOR imm32.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr XOR_rm32_imm32{{0U, REX::NONE, B8F::NONE, OPCodeExt::i6, 0x81U}, ArgType::rm32, ArgType::imm32, false, true};
/// @brief XOR r/m64, imm32: r/m64 XOR imm32 (sign-extended).
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr XOR_rm64_imm32sx{{0U, REX::W, B8F::NONE, OPCodeExt::i6, 0x81U}, ArgType::rm64, ArgType::imm32sx_64, false, true};
/// @brief XOR r/m32, r32: r/m32 XOR r32.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr XOR_rm32_r32{{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x31U}, ArgType::rm32, ArgType::r32, false, true};
/// @brief XOR r/m64, r64: r/m64 XOR r64.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr XOR_rm64_r64{{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x31U}, ArgType::rm64, ArgType::r64, false, true};
/// @brief 	XOR r32, r/m32: r32 XOR r/m32.
constexpr AbstrInstr XOR_r32_rm32{{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x33U}, ArgType::r32, ArgType::rm32, false, true};
/// @brief XOR r64, r/m64: r64 XOR r/m64.
constexpr AbstrInstr XOR_r64_rm64{{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x33U}, ArgType::r64, ArgType::rm64, false, true};

/// @brief SHL r/m32,1: Multiply r/m32 by 2, once.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SHL_rm32_1{{0U, REX::NONE, B8F::NONE, OPCodeExt::i4, 0xD1U}, ArgType::rm32, ArgType::c1_32, false, false};
/// @brief SHL r/m64,1: Multiply r/m64 by 2, once.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SHL_rm64_1{{0U, REX::W, B8F::NONE, OPCodeExt::i4, 0xD1U}, ArgType::rm64, ArgType::c1_64, false, false};
/// @brief SHL r/m32, imm8: Multiply r/m32 by 2, imm8 times.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SHL_rm32_imm8{{0U, REX::NONE, B8F::NONE, OPCodeExt::i4, 0xC1U}, ArgType::rm32, ArgType::imm8_32, false, false};
/// @brief SHL r/m64, imm8: Multiply r/m64 by 2, imm8 times.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SHL_rm64_imm8{{0U, REX::W, B8F::NONE, OPCodeExt::i4, 0xC1U}, ArgType::rm64, ArgType::imm8_64, false, false};
/// @brief SHR r/m32, CL: Unsigned divide r/m32 by 2, CL times.
/// NOTE: Register CL must be loaded manually (omit CL)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SHL_rm32_omit_CL{{0U, REX::NONE, B8F::NONE, OPCodeExt::i4, 0xD3U}, ArgType::rm32, ArgType::NONE, true, false};
/// @brief SHR r/m64, CL: Unsigned divide r/m64 by 2, CL times.
/// NOTE: Register CL must be loaded manually (omit CL)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SHL_rm64_omit_CL{{0U, REX::W, B8F::NONE, OPCodeExt::i4, 0xD3U}, ArgType::rm64, ArgType::NONE, true, false};

/// @brief SHR r/m32, 1: Unsigned divide r/m32 by 2, once.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SHR_rm32_1{{0U, REX::NONE, B8F::NONE, OPCodeExt::i5, 0xD1U}, ArgType::rm32, ArgType::c1_32, false, false};
/// @brief SHR r/m64, 1: Unsigned divide r/m64 by 2, once.
constexpr AbstrInstr SHR_rm64_1{{0U, REX::W, B8F::NONE, OPCodeExt::i5, 0xD1U}, ArgType::rm64, ArgType::c1_64, false, false};
/// @brief SHR r/m32, imm8: Unsigned divide r/m32 by 2, imm8 times.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SHR_rm32_imm8{{0U, REX::NONE, B8F::NONE, OPCodeExt::i5, 0xC1U}, ArgType::rm32, ArgType::imm8_32, false, false};
/// @brief SHR r/m64, imm8: Unsigned divide r/m64 by 2, imm8 times.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SHR_rm64_imm8{{0U, REX::W, B8F::NONE, OPCodeExt::i5, 0xC1U}, ArgType::rm64, ArgType::imm8_64, false, false};
/// @brief SHR r/m32, CL: Unsigned divide r/m32 by 2, CL times.
/// NOTE: Register CL must be loaded manually (omit CL)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SHR_rm32_omit_CL{{0U, REX::NONE, B8F::NONE, OPCodeExt::i5, 0xD3U}, ArgType::rm32, ArgType::NONE, true, false};
/// @brief SHR r/m64, CL: Unsigned divide r/m64 by 2, CL times.
/// NOTE: Register CL must be loaded manually (omit CL)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SHR_rm64_omit_CL{{0U, REX::W, B8F::NONE, OPCodeExt::i5, 0xD3U}, ArgType::rm64, ArgType::NONE, true, false};

/// @brief SAR r/m32, 1: Signed divide r/m32 by 2, once.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SAR_rm32_1{{0U, REX::NONE, B8F::NONE, OPCodeExt::i7, 0xD1U}, ArgType::rm32, ArgType::c1_32, false, false};
/// @brief SAR r/m64, 1: Signed divide r/m64 by 2, once.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SAR_rm64_1{{0U, REX::W, B8F::NONE, OPCodeExt::i7, 0xD1U}, ArgType::rm64, ArgType::c1_64, false, false};
/// @brief SAR r/m32, imm8: Signed divide r/m32 by 2, imm8 times.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SAR_rm32_imm8{{0U, REX::NONE, B8F::NONE, OPCodeExt::i7, 0xC1U}, ArgType::rm32, ArgType::imm8_32, false, false};
/// @brief SAR r/m64, imm8, Signed divide r/m64 by 2, imm8 times
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SAR_rm64_imm8{{0U, REX::W, B8F::NONE, OPCodeExt::i7, 0xC1U}, ArgType::rm64, ArgType::imm8_64, false, false};
/// @brief SAR r/m32, CL: Signed divide r/m32 by 2, CL times.
/// NOTE: Register CL must be loaded manually (omit CL)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SAR_rm32_omit_CL{{0U, REX::NONE, B8F::NONE, OPCodeExt::i7, 0xD3U}, ArgType::rm32, ArgType::NONE, true, false};
/// @brief SAR r/m64, CL: Signed divide r/m64 by 2, CL times.
/// NOTE: Register CL must be loaded manually (omit CL)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SAR_rm64_omit_CL{{0U, REX::W, B8F::NONE, OPCodeExt::i7, 0xD3U}, ArgType::rm64, ArgType::NONE, true, false};

/// @brief ROL r/m32, 1: Rotate 32 bits r/m32 left once.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ROL_rm32_1{{0U, REX::NONE, B8F::NONE, OPCodeExt::i0, 0xD1U}, ArgType::rm32, ArgType::c1_32, false, false};
/// @brief ROL r/m64, 1: Rotate 64 bits r/m64 left once. Uses a 6 bit count.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ROL_rm64_1{{0U, REX::W, B8F::NONE, OPCodeExt::i0, 0xD1U}, ArgType::rm64, ArgType::c1_64, false, false};
/// @brief ROL r/m32, imm8: Rotate 32 bits r/m32 left imm8 times.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ROL_rm32_imm8{{0U, REX::NONE, B8F::NONE, OPCodeExt::i0, 0xC1U}, ArgType::rm32, ArgType::imm8_32, false, false};
/// @brief ROL r/m64, imm8: Rotate 64 bits r/m64 left imm8 times. Uses a 6 bit count.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ROL_rm64_imm8{{0U, REX::W, B8F::NONE, OPCodeExt::i0, 0xC1U}, ArgType::rm64, ArgType::imm8_64, false, false};
/// @brief ROL r/m32, CL: Rotate 32 bits r/m32 left CL times.
/// NOTE: Register CL must be loaded manually (omit CL)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ROL_rm32_omit_CL{{0U, REX::NONE, B8F::NONE, OPCodeExt::i0, 0xD3U}, ArgType::rm32, ArgType::NONE, true, false};
/// @brief ROL r/m64, CL: Rotate 64 bits r/m64 left CL times. Uses a 6 bit count.
/// NOTE: Register CL must be loaded manually (omit CL)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ROL_rm64_omit_CL{{0U, REX::W, B8F::NONE, OPCodeExt::i0, 0xD3U}, ArgType::rm64, ArgType::NONE, true, false};

/// @brief ROR r/m32, 1: Rotate 32 bits r/m32 right once.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ROR_rm32_1{{0U, REX::NONE, B8F::NONE, OPCodeExt::i1, 0xD1U}, ArgType::rm32, ArgType::c1_32, false, false};
/// @brief ROR r/m64, 1: Rotate 64 bits r/m64 right once. Uses a 6 bit count.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ROR_rm64_1{{0U, REX::W, B8F::NONE, OPCodeExt::i1, 0xD1U}, ArgType::rm64, ArgType::c1_64, false, false};
/// @brief ROR r/m32, imm8: Rotate 32 bits r/m32 right imm8 times.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ROR_rm32_imm8{{0U, REX::NONE, B8F::NONE, OPCodeExt::i1, 0xC1U}, ArgType::rm32, ArgType::imm8_32, false, false};
/// @brief ROR r/m64, imm8: Rotate 64 bits r/m64 right imm8 times. Uses a 6 bit count.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ROR_rm64_imm8{{0U, REX::W, B8F::NONE, OPCodeExt::i1, 0xC1U}, ArgType::rm64, ArgType::imm8_64, false, false};
/// @brief ROR r/m32, CL: Rotate 32 bits r/m32 right CL times.
/// NOTE: Register CL must be loaded manually (omit CL)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ROR_rm32_omit_CL{{0U, REX::NONE, B8F::NONE, OPCodeExt::i1, 0xD3U}, ArgType::rm32, ArgType::NONE, true, false};
/// @brief ROR r/m64, CL: Rotate 64 bits r/m64 right CL times. Uses a 6 bit count.
/// NOTE: Register CL must be loaded manually (omit CL)
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr ROR_rm64_omit_CL{{0U, REX::W, B8F::NONE, OPCodeExt::i1, 0xD3U}, ArgType::rm64, ArgType::NONE, true, false};

/// @brief IDIV r/m32: Signed divide EDX:EAX by r/m32, with result stored in EAX ← Quotient, EDX ← Remainder.
/// NOTE: Registers EDX and EAX must be loaded manually
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr IDIV_rm32{{0U, REX::NONE, B8F::NONE, OPCodeExt::i7, 0xF7U}, ArgType::NONE, ArgType::rm32, true, false};
/// @brief IDIV r/m64: Signed divide RDX:RAX by r/m64, with result stored in RAX ← Quotient, RDX ← Remainder.
/// NOTE: Registers EDX and EAX must be loaded manually
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr IDIV_rm64{{0U, REX::W, B8F::NONE, OPCodeExt::i7, 0xF7U}, ArgType::NONE, ArgType::rm64, true, false};
/// @brief DIV r/m32: Unsigned divide EDX:EAX by r/m32, with result stored in EAX ← Quotient, EDX ← Remainder.
/// NOTE: Registers EDX and EAX must be loaded manually
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr DIV_rm32{{0U, REX::NONE, B8F::NONE, OPCodeExt::i6, 0xF7U}, ArgType::NONE, ArgType::rm32, true, false};
/// @brief DIV r/m64: Unsigned divide RDX:RAX by r/m64, with result stored in RAX ← Quotient, RDX ← Remainder.
/// NOTE: Registers EDX and EAX must be loaded manually
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr DIV_rm64{{0U, REX::W, B8F::NONE, OPCodeExt::i6, 0xF7U}, ArgType::NONE, ArgType::rm64, true, false};

/// @brief IMUL r32, r/m32, imm8: doubleword register ← r/m32 ∗ sign-extended immediate byte.
/// NOTE: Immediate will not be emitted. Must be done manually.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr IMUL_r32_rm32_omit_imm8sx{{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x6BU}, ArgType::r32, ArgType::rm32, true, false};
/// @brief IMUL r64, r/m64, imm8: quadword register ← r/m64 ∗ sign-extended immediate byte.
/// NOTE: Immediate will not be emitted. Must be done manually.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr IMUL_r64_rm64_omit_imm8sx{{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x6BU}, ArgType::r64, ArgType::rm64, true, false};
/// @brief IMUL r32, r/m32, imm32: doubleword register ← r/m32 ∗ immediate doubleword.
/// NOTE: Immediate will not be emitted. Must be done manually.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr IMUL_r32_rm32_omit_imm32{{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x69U}, ArgType::r32, ArgType::rm32, true, false};
/// @brief IMUL r64, r/m64, imm32: quadword register ← r/m64 ∗ immediate doubleword.
/// NOTE: Immediate will not be emitted. Must be done manually.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr IMUL_r64_rm64_omit_imm32sx{{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x69U}, ArgType::r64, ArgType::rm64, true, false};
/// @brief IMUL r32, r/m32: doubleword register ← doubleword register ∗ r/m32.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr IMUL_r32_rm32{{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0FAFU}, ArgType::r32, ArgType::rm32, false, true};
/// @brief IMUL r64, r/m64: quadword register ← quadword register ∗ r/m64.
constexpr AbstrInstr IMUL_r64_rm64{{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x0FAFU}, ArgType::r64, ArgType::rm64, false, true};

/// @brief NEG r/rm64: Two's complement negate r/m64.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate NEG_rm64{0U, REX::W, B8F::NONE, OPCodeExt::i3, 0xF7U};

/// @brief CMP r/rm8, imm8: Compare imm8 with r/rm8.
constexpr OPCodeTemplate CMP_rm8_imm8{0U, REX::NONE, B8F::NONE, OPCodeExt::i7, 0x80U};
/// @brief MOV r/m8, imm8: Move imm8 to r/m8.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOV_rm8_imm8_t{0U, REX::NONE, B8F::RM, OPCodeExt::i0, 0xC6U};
/// @brief MOV r/m8,r8: Move r8 to r/m8.
constexpr OPCodeTemplate MOV_rm8_r8_t{0U, REX::NONE, B8F::RM | B8F::R, OPCodeExt::R, 0x88U};
/// @brief MOV r8,r/m8: Move r/m8 to r8.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOV_r8_rm8_t{0U, REX::NONE, B8F::RM | B8F::R, OPCodeExt::R, 0x8AU};

/// @brief MOV r32, imm32: Move imm32 to r32.
constexpr OPCodeTemplate MOV_r32_imm32{0U, REX::NONE, B8F::NONE, OPCodeExt::RADD, 0xB8U};

/// @brief MOV r16, imm16: Move imm16 to r16.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOV_rm16_imm16_t{0x66U, REX::NONE, B8F::NONE, OPCodeExt::i0, 0xC7U};
/// @brief MOV r32, imm32: Move imm32 to r32.
constexpr AbstrInstr MOV_rm32_imm32{{0U, REX::NONE, B8F::NONE, OPCodeExt::i0, 0xC7U}, ArgType::rm32, ArgType::imm32, true, false};
/// @brief MOV r/m64, imm32: Move imm32 sign extended to 64-bits to r/m64.
constexpr AbstrInstr MOV_rm64_imm32sx{{0U, REX::W, B8F::NONE, OPCodeExt::i0, 0xC7U}, ArgType::rm64, ArgType::imm32sx_64, true, false};
/// @brief MOV r/m16,r16: Move r16 to r/m16.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOV_rm16_r16_t{0x66U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x89U};
/// @brief MOV r/m32,r32: Move r32 to r/m32.
constexpr AbstrInstr MOV_rm32_r32{{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x89U}, ArgType::rm32, ArgType::r32, true, false};
/// @brief MOV r/m64,r64: Move r64 to r/m64.
constexpr AbstrInstr MOV_rm64_r64{{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x89U}, ArgType::rm64, ArgType::r64, true, false};
/// @brief MOV r32,r/m32: Move r/m32 to r32.
constexpr AbstrInstr MOV_r32_rm32{{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x8BU}, ArgType::r32, ArgType::rm32, true, false};
/// @brief MOV r64,r/m64: Move r/m64 to r64.
constexpr AbstrInstr MOV_r64_rm64{{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x8BU}, ArgType::r64, ArgType::rm64, true, false};

/// @brief MOVSX r32, r/m8: Move byte to doubleword with sign-extension.
constexpr OPCodeTemplate MOVSX_r32_rm8_t{0U, REX::NONE, B8F::RM, OPCodeExt::R, 0x0FBEU};
/// @brief MOVSX r64, r/m8: Move byte to quadword with sign-extension.
constexpr OPCodeTemplate MOVSX_r64_rm8_t{0U, REX::W, B8F::RM, OPCodeExt::R, 0x0FBEU};
/// @brief MOVSX r32, r/m16: Move word to doubleword, with sign-extension.
constexpr OPCodeTemplate MOVSX_r32_rm16_t{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0FBFU};
/// @brief MOVSX r64, r/m16: Move word to quadword with sign-extension.
constexpr OPCodeTemplate MOVSX_r64_rm16_t{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x0FBFU};

/// @brief MOVSXD r64, r/m32: Move doubleword to quadword with sign-extension.
constexpr AbstrInstr MOVSXD_r64_rm32{{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x63U}, ArgType::r64, ArgType::rm32, true, false};

/// @brief MOVZX r32, r/m8: Move byte to doubleword, zero-extension.
constexpr OPCodeTemplate MOVZX_r32_rm8_t{0U, REX::NONE, B8F::RM, OPCodeExt::R, 0x0FB6U};
/// @brief MOVZX r64, r/m8: Move byte to quadword, zero-extension.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOVZX_r64_rm8_t{0U, REX::W, B8F::RM, OPCodeExt::R, 0x0FB6U};
/// @brief MOVZX r32, r/m16: Move word to doubleword, zero-extension.
constexpr OPCodeTemplate MOVZX_r32_rm16_t{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0FB7U};
/// @brief MOVZX r64, r/m16: Move word to quadword, zero-extension.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOVZX_r64_rm16_t{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x0FB7U};

/// @brief MOVD xmm, r/m32: Move doubleword from r/m32 to xmm.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr MOVD_rf_rm32{{0x66U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F6EU}, ArgType::r32f, ArgType::rm32, true, false};
/// @brief MOVQ xmm, r/m64: Move quadword from r/m64 to xmm.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr MOVQ_rf_rm64{{0x66U, REX::W, B8F::NONE, OPCodeExt::R, 0x0F6EU}, ArgType::r64f, ArgType::rm64, true, false};
/// @brief MOVD r/m32, xmm: Move doubleword from xmm register to r/m32.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr MOVD_rm32_rf{{0x66U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F7EU}, ArgType::rm32, ArgType::r32f, true, false};
/// @brief MOVQ r/m64, xmm: Move quadword from xmm register to r/m64.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr MOVQ_rm64_rf{{0x66U, REX::W, B8F::NONE, OPCodeExt::R, 0x0F7EU}, ArgType::rm64, ArgType::r64f, true, false};

/// @brief PSRLD xmm1, imm8: Shift doublewords in xmm1 right by imm8 while shifting in 0s.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr PSRLD_rf_imm8{{0x66U, REX::NONE, B8F::NONE, OPCodeExt::i2, 0x0F72U}, ArgType::r32f, ArgType::imm8_32, false, false};
/// @brief PSRLQ xmm1, imm8: Shift quadwords in xmm1 right by imm8 while shifting in 0s.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr PSRLQ_rf_imm8{{0x66U, REX::NONE, B8F::NONE, OPCodeExt::i2, 0x0F73U}, ArgType::r64f, ArgType::imm8_32, false, false};
/// @brief PSLLD xmm1, imm8: Shift doublewords in xmm1 left by imm8 while shifting in 0s.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr PSLLD_rf_imm8{{0x66U, REX::NONE, B8F::NONE, OPCodeExt::i6, 0x0F72U}, ArgType::r32f, ArgType::imm8_32, false, false};
/// @brief PSLLQ xmm1, imm8: Shift quadwords in xmm1 left by imm8 while shifting in 0s.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr PSLLQ_rf_imm8{{0x66U, REX::NONE, B8F::NONE, OPCodeExt::i6, 0x0F73U}, ArgType::r64f, ArgType::imm8_32, false, false};

/// @brief XORPS xmm1, xmm2/m128: Return the bitwise logical XOR of packed single-precision floating-point values in
/// xmm1 and xmm2/mem.
constexpr AbstrInstr XORPS_rf_rmf{{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F57U}, ArgType::r32f, ArgType::rm32f_128_restrictm, false, true};
/// @brief XORPD xmm1, xmm2/m128: Return the bitwise logical XOR of packed double-precision floating-point values in
/// xmm1 and xmm2/mem.
constexpr AbstrInstr XORPD_rf_rmf{{0x66U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F57U}, ArgType::r64f, ArgType::rm64f_128_restrictm, false, true};
/// @brief ANDPS xmm1, xmm2/m128: Return the bitwise logical AND of packed single-precision floating-point values in
/// xmm1 and xmm2/mem.
constexpr AbstrInstr ANDPS_rf_rmf{{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F54U}, ArgType::r32f, ArgType::rm32f_128_restrictm, false, true};
/// @brief ANDPD xmm1, xmm2/m128: Return the bitwise logical AND of packed double-precision floating-point values in
/// xmm1 and xmm2/mem.
constexpr AbstrInstr ANDPD_rf_rmf{{0x66U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F54U}, ArgType::r64f, ArgType::rm64f_128_restrictm, false, true};
/// @brief ORPS xmm1, xmm2/m128: Return the bitwise logical OR of packed single-precision floating-point values in xmm1
/// and xmm2/mem.
constexpr AbstrInstr ORPS_rf_rmf{{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F56U}, ArgType::r32f, ArgType::rm32f_128_restrictm, false, true};
/// @brief ORPD xmm1, xmm2/m128: Return the bitwise logical OR of packed double-precision floating-point values in xmm1
/// and xmm2/mem.
constexpr AbstrInstr ORPD_rf_rmf{{0x66U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F56U}, ArgType::r64f, ArgType::rm64f_128_restrictm, false, true};

/// @brief MOVSS xmm1, xmm2/m32: Merge scalar single-precision floating-point value from xmm2/m32 to xmm1 register.
constexpr AbstrInstr MOVSS_rf_rmf{{0xF3U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F10U}, ArgType::r32f, ArgType::rm32f, true, false};
/// @brief MOVSD xmm1, xmm2/m32: Move scalar double-precision floating-point value from xmm2/m32 to xmm1 register.
constexpr AbstrInstr MOVSD_rf_rmf{{0xF2U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F10U}, ArgType::r64f, ArgType::rm64f, true, false};
/// @brief MOVSS xmm2/m32, xmm1: Move scalar single-precision floating-point value from xmm1 register to xmm2/m32.
constexpr AbstrInstr MOVSS_rmf_rf{{0xF3U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F11U}, ArgType::r32f, ArgType::rm32f, true, false};
/// @brief MOVSD xmm1/m64, xmm2: Move scalar double-precision floating-point value from xmm2 register to xmm1/m64.
constexpr AbstrInstr MOVSD_rmf_rf{{0xF2U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F11U}, ArgType::r64f, ArgType::rm64f, true, false};

/// @brief ROUNDSS xmm1, xmm2/m32, imm8: Round the low packed single precision floating-point value in xmm2/m32 and
/// place the result in xmm1. The rounding mode is determined by imm8. NOTE: Immediate will not be emitted. Must be done
/// manually.
constexpr AbstrInstr ROUNDSS_rf_rmf_omit_imm8{{0x66U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F3A0AU}, ArgType::r32f, ArgType::rm32f, true, false};
/// @brief ROUNDSD xmm1, xmm2/m64, imm8: Round the low packed double precision floating-point value in xmm2/m64 and
/// place the result in xmm1. The rounding mode is determined by imm8. NOTE: Immediate will not be emitted. Must be done
/// manually.
constexpr AbstrInstr ROUNDSD_rf_rmf_omit_imm8{{0x66U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F3A0BU}, ArgType::r64f, ArgType::rm64f, true, false};
/// @brief SQRTSS xmm1, xmm2/m32: Computes square root of the low single-precision floating-point value in xmm2/m32 and
/// stores the results in xmm1.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SQRTSS_rf_rmf{{0xF3U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F51U}, ArgType::r32f, ArgType::rm32f, true, false};
/// @brief SQRTSD xmm1, xmm2/m64: Computes square root of the low double-precision floating-point value in xmm2/m64 and
/// stores the results in xmm1.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr SQRTSD_rf_rmf{{0xF2U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F51U}, ArgType::r64f, ArgType::rm64f, true, false};
/// @brief ADDSS xmm1, xmm2/m32: Add the low single-precision floating-point value from xmm2/mem to xmm1 and store the
/// result in xmm1.
constexpr AbstrInstr ADDSS_rf_rmf{{0xF3U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F58U}, ArgType::r32f, ArgType::rm32f, false, true};
/// @brief ADDSD xmm1, xmm2/m64: Add the low double-precision floating-point value from xmm2/mem to xmm1 and store the
/// result in xmm1.
constexpr AbstrInstr ADDSD_rf_rmf{{0xF2U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F58U}, ArgType::r64f, ArgType::rm64f, false, true};
/// @brief SUBSS xmm1, xmm2/m32: Subtract the low single-precision floating-point value in xmm2/m32 from xmm1 and store
/// the result in xmm1.
constexpr AbstrInstr SUBSS_rf_rmf{{0xF3U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F5CU}, ArgType::r32f, ArgType::rm32f, false, false};
/// @brief SUBSD xmm1, xmm2/m64: Subtract the low double-precision floating-point value in xmm2/m64 from xmm1 and store
/// the result in xmm1.
constexpr AbstrInstr SUBSD_rf_rmf{{0xF2U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F5CU}, ArgType::r64f, ArgType::rm64f, false, false};
/// @brief MULSS xmm1,xmm2/m32: Multiply the low single-precision floating-point value in xmm2/m32 by the low
/// single-precision floating-point value in xmm1.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr MULSS_rf_rmf{{0xF3U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F59U}, ArgType::r32f, ArgType::rm32f, false, true};
/// @brief MULSD xmm1,xmm2/m64: Multiply the low double-precision floating-point value in xmm2/m64 by low
/// double-precision floating-point value in xmm1.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr MULSD_rf_rmf{{0xF2U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F59U}, ArgType::r64f, ArgType::rm64f, false, true};
/// @brief DIVSS xmm1, xmm2/m32: Divide low single-precision floating-point value in xmm1 by low single-precision
/// floating-point value in xmm2/m32.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr DIVSS_rf_rmf{{0xF3U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F5EU}, ArgType::r32f, ArgType::rm32f, false, false};
/// @brief DIVSD xmm1, xmm2/m64: Divide low double-precision floating-point value in xmm1 by low double-precision
/// floating-point value in xmm2/m64.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr DIVSD_rf_rmf{{0xF2U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F5EU}, ArgType::r64f, ArgType::rm64f, false, false};
/// @brief MINSS xmm1,xmm2/m32: Return the minimum scalar single-precision floating-point value between xmm2/m32 and
/// xmm1.
constexpr AbstrInstr MINSS_rf_rmf{{0xF3U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F5DU}, ArgType::r32f, ArgType::rm32f, false, true};
/// @brief MINSD xmm1, xmm2/m64: Return the minimum scalar double-precision floating-point value between xmm2/m64 and
/// xmm1.
constexpr AbstrInstr MINSD_rf_rmf{{0xF2U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F5DU}, ArgType::r64f, ArgType::rm64f, false, true};
/// @brief MAXSS xmm1, xmm2/m32: Return the maximum scalar single-precision floating-point value between xmm2/m32 and
/// xmm1.
constexpr AbstrInstr MAXSS_rf_rmf{{0xF2U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F5FU}, ArgType::r32f, ArgType::rm32f, false, true};
/// @brief MAXSD xmm1, xmm2/m64: Return the maximum scalar double-precision floating-point value between xmm2/m64 and
/// xmm1.
constexpr AbstrInstr MAXSD_rf_rmf{{0xF3U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F5FU}, ArgType::r64f, ArgType::rm64f, false, true};

/// @brief CVTSS2SI r32, xmm1/m32: Convert one single-precision floating-point value from xmm1/m32 to one signed
/// doubleword integer in r32.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr CVTSS2SI_r32_rmf{{0xF3U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F2DU}, ArgType::r32, ArgType::rm32f, true, false};
/// @brief CVTSS2SI r64, xmm1/m32: Convert one single-precision floating-point value from xmm1/m32 to one signed
/// quadword integer in r64.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr CVTSS2SI_r64_rmf{{0xF3U, REX::W, B8F::NONE, OPCodeExt::R, 0x0F2DU}, ArgType::r64, ArgType::rm32f, true, false};
/// @brief CVTSD2SI r32, xmm1/m64: Convert one double-precision floating-point value from xmm1/m64 to one signed
/// doubleword integer r32.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr CVTSD2SI_r32_rmf{{0xF2U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F2DU}, ArgType::r32, ArgType::rm64f, true, false};
/// @brief CVTSD2SI r64, xmm1/m64: Convert one double-precision floating-point value from xmm1/m64 to one signed
/// quadword integer sign-extended into r64.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr CVTSD2SI_r64_rmf{{0xF2U, REX::W, B8F::NONE, OPCodeExt::R, 0x0F2DU}, ArgType::r64, ArgType::rm64f, true, false};

/// @brief CVTSI2SS xmm1, r/m32: Convert one signed doubleword integer from r/m32 to one single-precision floating-point
/// value in xmm1.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr CVTSI2SS_rf_rm32{{0xF3U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F2AU}, ArgType::r32f, ArgType::rm32, true, false};
/// @brief CVTSI2SS xmm1, r/m64: Convert one signed quadword integer from r/m64 to one single-precision floating-point
/// value in xmm1.
constexpr AbstrInstr CVTSI2SS_rf_rm64{{0xF3U, REX::W, B8F::NONE, OPCodeExt::R, 0x0F2AU}, ArgType::r32f, ArgType::rm64, true, false};
/// @brief CVTSI2SD xmm1, r/m32: Convert one signed doubleword integer from r32/m32 to one double-precision
/// floating-point value in xmm1.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr CVTSI2SD_rf_rm32{{0xF2U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F2AU}, ArgType::r64f, ArgType::rm32, true, false};
/// @brief CVTSI2SD xmm1, r/m64: Convert one signed quadword integer from r/m64 to one double-precision floating-point
/// value in xmm1.
constexpr AbstrInstr CVTSI2SD_rf_rm64{{0xF2U, REX::W, B8F::NONE, OPCodeExt::R, 0x0F2AU}, ArgType::r64f, ArgType::rm64, true, false};

/// @brief CVTSS2SD xmm1, xmm2/m32: Convert one single-precision floating-point value in xmm2/m32 to one
/// double-precision floating-point value in xmm1.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr CVTSS2SD_rf_rmf{{0xF3U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F5AU}, ArgType::r64f, ArgType::rm32f, true, false};
/// @brief CVTSD2SS xmm1, xmm2/m64: Convert one double-precision floating-point value in xmm2/m64 to one
/// single-precision floating-point value in xmm1.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr AbstrInstr CVTSD2SS_rf_rmf{{0xF2U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F5AU}, ArgType::r32f, ArgType::rm64f, true, false};

/// @brief TEST r/m64, r64: AND r64 with r/m64; set SF, ZF, PF according to result.
constexpr OPCodeTemplate TEST_rm64_r64_t{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x85U};

/// @brief TEST r/m32, r32: AND r32 with r/m32; set SF, ZF, PF according to result.
constexpr OPCodeTemplate TEST_rm32_imm32_t{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0xF7U};
/// @brief XCHG r/m32, r32: Exchange r32 with doubleword from r/m32.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate XCHG_rm32_r32_t{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x87U};
/// @brief XCHG r64, r/m64: Exchange quadword from r/m64 with r64.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate XCHG_rm64_r64_t{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x87U};

/// @brief CDQ: EDX:EAX ← sign-extend of EAX.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate CDQ_t{0U, REX::NONE, B8F::NONE, OPCodeExt::NONE, 0x99U};
/// @brief CDO: RDX:RAX← sign-extend of RAX.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate CDO_t{0U, REX::W, B8F::NONE, OPCodeExt::NONE, 0x99U};

/// @brief RET: Near return to calling procedure.
constexpr OPCodeTemplate RET_t{0U, REX::NONE, B8F::NONE, OPCodeExt::NONE, 0xC3U};
/// @brief RET imm16: Near return to calling procedure and pop imm16 bytes from stack.
constexpr OPCodeTemplate RET_imm16_t{0U, REX::NONE, B8F::NONE, OPCodeExt::NONE, 0xC2U};

/// @brief CALL rel32: Call near, relative, displacement relative to next instruction. 32-bit displacement sign extended
/// to 64-bits in 64-bit mode.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate CALL_rel32_t{0U, REX::NONE, B8F::NONE, OPCodeExt::NONE, 0xE8U};
/// @brief CALL r/m64: Call near, absolute indirect, address given in r/m64.
constexpr OPCodeTemplate CALL_rm64_t{0U, REX::NONE, B8F::NONE, OPCodeExt::i2, 0xFFU};

/// @brief PUSH r64: Decrement stack pointer, push r64 onto top of stack.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate PUSH_r64_t{0U, REX::NONE, B8F::NONE, OPCodeExt::RADD, 0x50U};
/// @brief POP r64: Pop top of stack into r64; increment stack pointer.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate POP_r64_t{0U, REX::NONE, B8F::NONE, OPCodeExt::RADD, 0x58U};

/// @brief MOVAPD xmm1, xmm2/m128: Move aligned packed double-precision floating-point values from xmm2/mem to xmm1.
constexpr OPCodeTemplate MOVAPD_rf_rmf128_t{0x66U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F28U};
/// @brief PUNPCKLDQ xmm1, xmm2/m128: Interleave low-order doublewords from xmm1 and xmm2/m128 into xmm1.
constexpr OPCodeTemplate PUNPCKLDQ_rf_rmf128_t{0x66U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F62U};
/// @brief SUBPD xmm1, xmm2/m128: Subtract packed double-precision floating-point values in xmm2/mem from xmm1 and store
/// result in xmm1.
constexpr OPCodeTemplate SUBPD_rf_rmf128_t{0x66U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F5CU};
/// @brief UNPCKHPD xmm1, xmm2/m128: Unpacks and Interleaves double-precision floating-point values from high quadwords
/// of xmm1 and xmm2/m128.
constexpr OPCodeTemplate UNPCKHPD_rf_rmf128_t{0x66U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F15U};

/// @brief MOV r64, imm64: Move imm64 to r64.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate MOV_r64_imm64_t{0U, REX::W, B8F::NONE, OPCodeExt::RADD, 0xB8U};

/// @brief JMP rel8: Jump short, RIP = RIP + 8-bit displacement sign extended to 64-bits
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate JMP_rel8_t{0U, REX::NONE, B8F::NONE, OPCodeExt::NONE, 0xEBU};
/// @brief JCC rel8: Jump short if condition is satisfied
/// NOTE: Assemble to final opcode by adding the underlying CC value to the opcode
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate JCC_rel8_t{0U, REX::NONE, B8F::NONE, OPCodeExt::NONE, 0x70U};
/// @brief JMP rel32: Jump near, relative, RIP = RIP + 32-bit displacement sign extended to 64-bits
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate JMP_rel32_t{0U, REX::NONE, B8F::NONE, OPCodeExt::NONE, 0xE9U};
/// @brief JCC rel32: Jump near if condition is satisfied
/// NOTE: Assemble to final opcode by adding the underlying CC value to the opcode
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate JCC_rel32_t{0x0FU, REX::NONE, B8F::NONE, OPCodeExt::NONE, 0x80U};

/// @brief JMP r/m64: Jump near, absolute indirect, RIP = 64-Bit offset from register or memory.
constexpr OPCodeTemplate JMP_rm64_t{0U, REX::NONE, B8F::NONE, OPCodeExt::i4, 0xFFU};

/// @brief CMOVCC r32, r/m32: Move if condition is satisfied
/// NOTE: Assemble to final opcode by adding the underlying CC value to the opcode
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate CMOVCC_r32_rm32_t{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x0F40U};
/// @brief CMOVCC r64, r/m64: Move if condition is satisfied
/// NOTE: Assemble to final opcode by adding the underlying CC value to the opcode
constexpr OPCodeTemplate CMOVCC_r64_rm64_t{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x0F40U};

/// @brief LEA r32,m: Store effective address for m in register r32.
constexpr OPCodeTemplate LEA_r32_m_t{0U, REX::NONE, B8F::NONE, OPCodeExt::R, 0x8DU};
/// @brief LEA r64,m: Store effective address for m in register r64.
constexpr OPCodeTemplate LEA_r64_m_t{0U, REX::W, B8F::NONE, OPCodeExt::R, 0x8DU};

/// @brief STMXCSR m32: Store contents of MXCSR register to m32.
constexpr OPCodeTemplate STMXCSR_m32_t{0U, REX::NONE, B8F::NONE, OPCodeExt::i3, 0x0FAEU};
/// @brief LDMXCSR m32: Load MXCSR register from m32.
constexpr OPCodeTemplate LDMXCSR_m32_t{0U, REX::NONE, B8F::NONE, OPCodeExt::i2, 0x0FAEU};

/// @brief BTC r/m64, imm8: Store selected bit in CF flag and complement.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate BTC_rm64_imm8_t{0U, REX::W, B8F::NONE, OPCodeExt::i7, 0x0FBAU};

/// @brief LAHF: Load: AH ← EFLAGS(SF:ZF:0:AF:0:PF:1:CF).
constexpr OPCodeTemplate LAHF_T{0U, REX::NONE, B8F::NONE, OPCodeExt::NONE, 0x9FU};
/// @brief SAHF: Loads SF, ZF, AF, PF, and CF from AH into EFLAGS register.
constexpr OPCodeTemplate SAHF_T{0U, REX::NONE, B8F::NONE, OPCodeExt::NONE, 0x9EU};

/// @brief Setcc instruction, takes 8 bit register as operand.
// coverity[autosar_cpp14_m3_4_1_violation]
constexpr OPCodeTemplate SETCC_rm8{0U, REX::NONE, B8F::NONE, OPCodeExt::i0, 0x0F90U};

/// @brief NOP (1-byte): Single-byte no-operation instruction.
// coverity[autosar_cpp14_m3_4_1_violation]
// coverity[single_use]
constexpr OPCodeTemplate NOP{0U, REX::NONE, B8F::NONE, OPCodeExt::NONE, 0x90U};

///
/// @brief Instruction generator for MOV from mem/reg to reg so the inputs can be comfortably switched in a single line
///
/// @param isGPR Whether a GPR or an FPR should be loaded
/// @param is64 Target register size
/// @return AbstrInstr Resulting AbstrInstr
inline AbstrInstr MOV_r_rm(const bool isGPR, const bool is64) VB_NOEXCEPT {
  if (isGPR) {
    return is64 ? MOV_r64_rm64 : MOV_r32_rm32;
  }
  return is64 ? MOVSD_rf_rmf : MOVSS_rf_rmf;
}

///
/// @brief Instruction generator for MOV from reg to mem/reg so the inputs can be comfortably switched in a single line
///
/// @param isGPR Whether a GPR or an FPR should be stored
/// @param is64 Target register size
/// @return AbstrInstr Resulting AbstrInstr
inline AbstrInstr MOV_rm_r(const bool isGPR, const bool is64) VB_NOEXCEPT {
  if (isGPR) {
    return is64 ? MOV_rm64_r64 : MOV_rm32_r32;
  }
  return is64 ? MOVSD_rmf_rf : MOVSS_rmf_rf;
}

} // namespace x86_64
} // namespace vb

#endif

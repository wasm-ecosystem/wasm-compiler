///
/// @file aarch64_instruction.hpp
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
#ifndef AARCH64_INSTRUCTION_HPP
#define AARCH64_INSTRUCTION_HPP

#include <cstdint>

#include "aarch64_encoding.hpp"
#include "aarch64_relpatchobj.hpp"

#include "src/core/common/util.hpp"
#include "src/core/compiler/common/MemWriter.hpp"
#include "src/core/compiler/common/SafeInt.hpp"

namespace vb {
namespace aarch64 {

///
/// @brief Instruction class used to assemble and encode a specific AArch64 instruction and then write it to an output
/// binary
///
class Instruction final {
public:
  ///
  /// @brief Construct a new Instruction instance from an OPCodeTemplate
  ///
  /// @param opcode OPCode template
  /// @param binary Output binary
  Instruction(OPCodeTemplate const opcode, MemWriter &binary) VB_NOEXCEPT;

  ///
  /// @brief Construct a new Instruction instance from an AbstrInstr
  ///
  /// @param abstrInstr Abstract instruction representation
  /// @param binary Output binary
  Instruction(AbstrInstr const abstrInstr, MemWriter &binary) VB_NOEXCEPT;

  ///
  /// @brief Construct a new Instruction with the copy operator
  ///
  Instruction(Instruction &) VB_NOEXCEPT = default;

  ///
  /// @brief Construct a new Instruction with the move operator
  ///
  Instruction(Instruction &&) VB_NOEXCEPT = default;
  Instruction &operator=(Instruction const &) & = delete;
  Instruction &operator=(Instruction &&) & = delete;

  ///
  /// @brief Destructor of the instruction instance that will check whether it was (probably) forgotten to emit this
  /// instruction via assert
  ///
  ~Instruction() VB_NOEXCEPT;

  ///
  /// @brief Set a register to the D field
  ///
  /// @param reg Register to encode
  /// @return Instruction&
  Instruction &setD(REG const reg) VB_NOEXCEPT;

  ///
  /// @brief Set a register to the T field
  ///
  /// @param reg Register to encode
  /// @return Instruction&
  Instruction &setT(REG const reg) VB_NOEXCEPT;

  ///
  /// @brief Set a register to the T1 field
  ///
  /// @param reg Register to encode
  /// @return Instruction&
  Instruction &setT1(REG const reg) VB_NOEXCEPT;

  ///
  /// @brief Set a register to the T2 field
  ///
  /// @param reg Register to encode
  /// @return Instruction&
  Instruction &setT2(REG const reg) VB_NOEXCEPT;

  ///
  /// @brief Set a register to the N field
  ///
  /// @param reg Register to encode
  /// @return Instruction&
  Instruction &setN(REG const reg) VB_NOEXCEPT;

  ///
  /// @brief Get a register from the N field
  ///
  /// @return REG from the N field
  REG getN() const VB_NOEXCEPT;

  ///
  /// @brief Clear the N register field
  ///
  /// @return Instruction&
  Instruction &clearN() VB_NOEXCEPT;

  ///
  /// @brief Set a register to the M field
  ///
  /// @param reg Register to encode
  /// @return Instruction&
  Instruction &setM(REG const reg) VB_NOEXCEPT;

  ///
  /// @brief Set a register to the A field
  ///
  /// @param reg Register to encode
  /// @return Instruction&
  Instruction &setA(REG const reg) VB_NOEXCEPT;

  ///
  /// @brief Set an unsigned 6-bit immediate to the imm6 field
  ///
  /// @param imm6 Immediate to encode
  /// @return Instruction&
  inline Instruction &setImm6(SafeUInt<6U> const imm6) VB_NOEXCEPT {
    return setImm6(imm6.value());
  }

  ///
  /// @brief Set an unsigned 6-bit immediate to the imm6x field
  ///
  /// This is used to encode rotate and shift instructions
  ///
  /// @param left Whether this is a left rotate or shift instruction
  /// @param imm6x Immediate to encode
  /// @return Instruction&
  inline Instruction &setImm6x(bool const left, SafeUInt<6U> const imm6x) VB_NOEXCEPT {
    return setImm6x(left, imm6x.value());
  }

  ///
  /// @brief Set an operand-size-scaled signed 11-bit immediate to the scSImm7 field as imm>>4
  ///
  /// @param imm Scaled immediate to encode
  /// @return Instruction&
  inline Instruction &setSImm7ls4(SafeInt<11> const imm) VB_NOEXCEPT {
    return setSImm7ls4(imm.value());
  }

  ///
  /// @brief Set an operand-size-scaled signed 10-bit immediate to the scSImm7 field as imm>>3
  ///
  /// @param imm Scaled immediate to encode
  /// @return Instruction&
  inline Instruction &setSImm7ls3(SafeInt<10> const imm) VB_NOEXCEPT {
    return setSImm7ls3(imm.value());
  }

  ///
  /// @brief Set an operand-size-scaled signed 9-bit immediate to the scSImm7 field as imm>>2
  ///
  /// @param imm Scaled immediate to encode
  /// @return Instruction&
  inline Instruction &setSImm7ls2(SafeInt<9> const imm) VB_NOEXCEPT {
    return setSImm7ls2(imm.value());
  }

  ///
  /// @brief Set a zero-extended 12-bit immediate  left shifted by 12 bits to the imm12zxols12 field
  ///
  /// @param imm Optionally shifted immediate to encode
  /// @return Instruction&
  inline Instruction &setImm12zxls12(SafeUInt<24> const imm) VB_NOEXCEPT {
    return setImm12zxls12(imm.value());
  }

  ///
  /// @brief Converts an immediate to a bitmask and sets it to the bitmask field
  /// NOTE: Undefined behavior if the immediate cannot be encoded as a bitmask
  ///
  /// @param imm Raw immediate (not yet converted to a bitmask)
  /// @return Instruction&
  Instruction &setImmBitmask(uint64_t const imm) VB_NOEXCEPT;

  ///
  /// @brief Set an already encoded immediate to the bitmask field
  ///
  /// @param encoding Bitmask representation of an immediate
  /// @return Instruction&
  Instruction &setRawImmBitmask(uint32_t const encoding) VB_NOEXCEPT;

  ///
  /// @brief Set an 16-bit immediate (optionally left shifted by 16, 32 or 48 bits) to the Imm16Ols field
  ///
  /// @param imm 16-bit immediate to be shifted and encoded
  /// @param shift By how many bits the immediate should be left shifted (Valid values: 0, 16, 32, 48)
  /// @return Instruction&
  inline Instruction &setImm16Ols(SafeUInt<16> const imm, uint32_t const shift) VB_NOEXCEPT {
    return setImm16Ols(imm.value(), shift);
  }

  ///
  /// @brief Set a float immediate to the FMOVImm8 field
  ///
  /// @param rawFloatImm Raw encoded float immediate
  /// @return Instruction&
  Instruction &setRawFMOVImm8(uint32_t const rawFloatImm) VB_NOEXCEPT;

  ///
  /// @brief Set a condition code to the corresponding field
  ///
  /// @param lowCond Whether the condition field is on the low (less significant) end of the instruction
  /// @param cc Condition code to encode
  /// @return Instruction&f
  Instruction &setCond(bool const lowCond, CC const cc) VB_NOEXCEPT;

  ///
  /// @brief Set an unsigned 12-bit immediate to the corresponding field
  ///
  /// @param imm Scaled 12-bit immediate
  /// @return Instruction&
  inline Instruction &setImm12zx(SafeUInt<12U> const imm) VB_NOEXCEPT {
    return setImm12zx(imm.value());
  }

  ///
  /// @brief Set an operand-scaled unsigned 13-bit immediate logical value, which will be encoded as physical value
  /// imm>>1 to represent the original logical value
  ///
  /// @param imm Scaled 13-bit immediate
  /// @return Instruction&
  inline Instruction &setImm12zxls1(SafeUInt<13U> const imm) VB_NOEXCEPT {
    return setImm12zxls1(imm.value());
  }

  ///
  /// @brief Set an operand-scaled unsigned 14-bit immediate logical value, which will be encoded as physical value
  /// imm>>2 to represent the original logical value
  ///
  /// @param imm Scaled 14-bit immediate
  /// @return Instruction&
  inline Instruction &setImm12zxls2(SafeUInt<14U> const imm) VB_NOEXCEPT {
    return setImm12zxls2(imm.value());
  }

  ///
  /// @brief Set an operand-scaled unsigned 15-bit immediate logical value, which will be encoded as physical value
  /// imm>>3 to represent the original logical value
  ///
  /// @param imm Scaled 15-bit immediate
  /// @return Instruction&
  inline Instruction &setImm12zxls3(SafeUInt<15U> const imm) VB_NOEXCEPT {
    return setImm12zxls3(imm.value());
  }

  ///
  /// @brief Set an unscaled signed 9-bit immediate to the corresponding field
  ///
  /// @param imm Signed immediate to encode
  /// @return Instruction&
  inline Instruction &setUnscSImm9(SafeInt<9U> const imm) VB_NOEXCEPT {
    return setUnscSImm9(imm.value());
  }

  ///
  /// @brief Set the 21-bit offset to the corresponding field
  /// NOTE: Which one will be set is automatically chosen depending on the instruction
  ///
  /// @param offset Offset from the start of this instruction to branch to
  /// @return Instruction&
  inline Instruction &setImm19ls2BranchOffset(SafeInt<21U> const offset) VB_NOEXCEPT {
    return setImm19ls2BranchOffset(offset.value());
  }

  ///
  /// @brief Set the 28-bit offset to the corresponding field
  /// NOTE: Which one will be set is automatically chosen depending on the instruction
  ///
  /// @param offset Offset from the start of this instruction to branch to
  /// @return Instruction&
  inline Instruction &setImm26ls2BranchOffset(SafeInt<28U> const offset) VB_NOEXCEPT {
    return setImm26ls2BranchOffset(offset.value());
  }

  ///
  /// @brief Set an optional left shift by up to 6 bits (for the 64-bit variant) or 5 bits (for the 32-bit variant) to
  /// the olsImm6 field
  ///
  /// @param count How many bits to shift the operand to the left (NOTE: Must be smaller than the operand size)
  /// @return Instruction&
  inline Instruction &setOlsImm6(SafeUInt<6> const count) VB_NOEXCEPT {
    return setOlsImm6(count.value());
  }

  ///
  /// @brief Set the 19-bit or 26-bit scaled offset to the corresponding field
  /// NOTE: Which one will be set is automatically chosen depending on the instruction
  ///
  /// @return Instruction&
  // coverity[autosar_cpp14_m9_3_3_violation]
  inline Instruction &setImm19o26ls2BranchPlaceHolder() VB_NOEXCEPT {
    return *this;
  }

  ///
  /// @brief Set a signed, unscaled 21-bit address offset to the corresponding field
  ///
  /// @param offset Offset from the current program counter
  /// @return Instruction&
  inline Instruction &setSigned21AddressOffset(SafeInt<21> const offset) VB_NOEXCEPT {
    return setSigned21AddressOffset(offset.value());
  }

  ///
  /// @brief Whether this branch instruction encodes a 19-bit (scaled) immediate branch offset
  /// CAUTION: Undefined behavior if this instruction is no relative-immediate branch instruction
  ///
  /// @return bool Whether this branch instruction encodes a 19-bit (scaled) immediate branch offset
  bool isImm19ls2BranchOffset() const VB_NOEXCEPT;

  ///
  /// @brief Read the 19-bit or 26-bit (scaled) immediate branch offset encoded in this branch instruction
  /// CAUTION: Undefined behavior if this instruction is no relative-immediate branch instruction
  ///
  /// @return int32_t The 19-bit or 26-bit (scaled) immediate branch offset
  int32_t readImm19o26ls2BranchOffset() const VB_NOEXCEPT;

  ///
  /// @brief Write the instruction to the output binary
  ///
  void emitCode();

  ///
  /// @brief Write the instruction to the output binary and return a corresponding RelPatchObj
  ///
  /// @return Corresponding RelPatchObj
  inline RelPatchObj prepJmp() {
    RelPatchObj const relPatchObj{RelPatchObj(binary_.size(), binary_)};
    emitCode();
    return relPatchObj;
  }

  ///
  /// @brief Short-hand operator for emitCode()
  ///
  inline void operator()() {
    emitCode();
  };

  ///
  /// @brief Manually set the status of this instruction to emitted
  ///
  /// @return Instruction&
  inline Instruction &setEmitted() VB_NOEXCEPT {
    emitted_ = true;
    return *this;
  }

  ///
  /// @brief Get the opcode of this instruction
  ///
  /// @return const OPCodeTemplate& Stored (not, partly or fully encoded) opcode
  inline OPCodeTemplate getOPCode() const VB_NOEXCEPT {
    return opcode_;
  }

  ///
  /// @brief Reset the opcode of this instruction
  ///
  /// @return Instruction&
  inline Instruction &resetOPCode(OPCodeTemplate const opcode) VB_NOEXCEPT {
    opcode_ = opcode;
    return *this;
  }

private:
  ///
  /// @brief Set an unsigned 6-bit immediate to the imm6 field
  ///
  /// @param imm6 Immediate to encode
  /// @return Instruction&
  Instruction &setImm6(uint32_t const imm6) VB_NOEXCEPT;

  ///
  /// @brief Set an unsigned 6-bit immediate to the imm6x field
  ///
  /// This is used to encode rotate and shift instructions
  ///
  /// @param left Whether this is a left rotate or shift instruction
  /// @param imm6x Immediate to encode
  /// @return Instruction&
  Instruction &setImm6x(bool const left, uint32_t const imm6x) VB_NOEXCEPT;

  ///
  /// @brief Set an operand-size-scaled signed 11-bit immediate to the scSImm7 field as imm>>4
  ///
  /// @param imm Scaled immediate to encode
  /// @return Instruction&
  Instruction &setSImm7ls4(int32_t const imm) VB_NOEXCEPT;

  ///
  /// @brief Set an operand-size-scaled signed 10-bit immediate to the scSImm7 field as imm>>3
  ///
  /// @param imm Scaled immediate to encode
  /// @return Instruction&
  Instruction &setSImm7ls3(int32_t const imm) VB_NOEXCEPT;

  ///
  /// @brief Set an operand-size-scaled signed 9-bit immediate to the scSImm7 field as imm>>2
  ///
  /// @param imm Scaled immediate to encode
  /// @return Instruction&
  Instruction &setSImm7ls2(int32_t const imm) VB_NOEXCEPT;

  ///
  /// @brief Set a zero-extended 12-bit immediate
  ///
  /// @param imm immediate to encode
  /// @return Instruction&
  Instruction &setImm12zx(uint32_t const imm) VB_NOEXCEPT;

  ///
  /// @brief Set a zero-extended 12-bit immediate  left shifted by 12 bits to the imm12zxols12 field
  ///
  /// @param imm Optionally shifted immediate to encode
  /// @return Instruction&
  Instruction &setImm12zxls12(uint32_t const imm) VB_NOEXCEPT;

  ///
  /// @brief Set an operand-scaled unsigned 13-bit immediate logical value, which will be encoded as physical value
  /// imm>>1 to represent the original logical value
  ///
  /// @param imm Scaled 13-bit immediate
  /// @return Instruction&
  Instruction &setImm12zxls1(uint32_t const imm) VB_NOEXCEPT;

  ///
  /// @brief Set an operand-scaled unsigned 14-bit immediate logical value, which will be encoded as physical value
  /// imm>>2 to represent the original logical value
  ///
  /// @param imm Scaled 14-bit immediate
  /// @return Instruction&
  Instruction &setImm12zxls2(uint32_t const imm) VB_NOEXCEPT;

  ///
  /// @brief Set an operand-scaled unsigned 15-bit immediate logical value, which will be encoded as physical value
  /// imm>>3 to represent the original logical value
  ///
  /// @param imm Scaled 15-bit immediate
  /// @return Instruction&
  Instruction &setImm12zxls3(uint32_t const imm) VB_NOEXCEPT;

  ///
  /// @brief Set an unscaled signed 9-bit immediate to the corresponding field
  ///
  /// @param imm Signed immediate to encode
  /// @return Instruction&
  Instruction &setUnscSImm9(int32_t const imm) VB_NOEXCEPT;

  ///
  /// @brief Set an 16-bit immediate (optionally left shifted by 16, 32 or 48 bits) to the Imm16Ols field
  ///
  /// @param imm 16-bit immediate to be shifted and encoded
  /// @param shift By how many bits the immediate should be left shifted (Valid values: 0, 16, 32, 48)
  /// @return Instruction&
  Instruction &setImm16Ols(uint32_t const imm, uint32_t const shift) VB_NOEXCEPT;

  ///
  /// @brief Set an optional left shift by up to 6 bits (for the 64-bit variant) or 5 bits (for the 32-bit variant) to
  /// the olsImm6 field
  ///
  /// @param count How many bits to shift the operand to the left (NOTE: Must be smaller than the operand size)
  /// @return Instruction&
  Instruction &setOlsImm6(uint32_t const count) VB_NOEXCEPT;

  ///
  /// @brief Set the 21-bit offset to the corresponding field
  /// NOTE: Which one will be set is automatically chosen depending on the instruction
  ///
  /// @param offset Offset from the start of this instruction to branch to
  /// @return Instruction&
  Instruction &setImm19ls2BranchOffset(int32_t const offset) VB_NOEXCEPT;

  ///
  /// @brief Set the 28-bit offset to the corresponding field
  /// NOTE: Which one will be set is automatically chosen depending on the instruction
  ///
  /// @param offset Offset from the start of this instruction to branch to
  /// @return Instruction&
  Instruction &setImm26ls2BranchOffset(int32_t const offset) VB_NOEXCEPT;

  ///
  /// @brief Set a signed, unscaled 21-bit address offset to the corresponding field
  ///
  /// @param offset Offset from the current program counter
  /// @return Instruction&
  Instruction &setSigned21AddressOffset(int32_t const offset) VB_NOEXCEPT;

  ///
  /// @brief The (not, partly or fully encoded) 4-byte OPCode of the instruction
  ///
  OPCodeTemplate opcode_;

  ///
  /// @brief Reference to the output binary
  ///
  MemWriter &binary_;

  ///
  /// @brief Whether this instruction has been emitted to the output binary
  ///
  bool emitted_;
};

} // namespace aarch64
} // namespace vb

#endif

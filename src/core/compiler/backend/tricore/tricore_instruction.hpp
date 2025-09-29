///
/// @file tricore_instruction.hpp
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
#ifndef TRICORE_INSTRUCTION_HPP
#define TRICORE_INSTRUCTION_HPP

#include <cstdint>

#include "tricore_encoding.hpp"
#include "tricore_relpatchobj.hpp"

#include "src/core/common/util.hpp"
#include "src/core/compiler/common/MemWriter.hpp"
#include "src/core/compiler/common/SafeInt.hpp"

namespace vb {
namespace tc {

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
  /// @brief Construct a new Instruction with the copy operator
  ///
  Instruction(Instruction &) VB_NOEXCEPT = default;

  ///
  /// @brief Construct a new Instruction with the move operator
  ///
  Instruction(Instruction &&) VB_NOEXCEPT = default;
  Instruction &operator=(Instruction const &) & = delete;
  Instruction &operator=(Instruction &&) & = delete;

#ifndef NDEBUG
  ///
  /// @brief Destructor of the instruction instance that will check whether it was (probably) forgotten to emit this
  /// instruction via assert
  ///
  ~Instruction() VB_NOEXCEPT {
    assert(emitted_ && "Instruction was created, but has not been emitted");
  }
#else
  ~Instruction() VB_NOEXCEPT = default;
#endif

  ///
  /// @brief Set a register to the Da field
  ///
  /// @param reg Register to encode
  /// @return Instruction&
  Instruction &setDa(REG const reg) VB_NOEXCEPT;

  ///
  /// @brief Set an extended register to the Ea field
  ///
  /// @param reg Register to encode
  /// @return Instruction&
  Instruction &setEa(REG const reg) VB_NOEXCEPT;

  ///
  /// @brief Set a register to the Aa field
  ///
  /// @param reg Register to encode
  /// @return Instruction&
  Instruction &setAa(REG const reg) VB_NOEXCEPT;

  ///
  /// @brief Set a register pair to the Pa field
  ///
  /// @param reg Register to encode
  /// @return Instruction&
  Instruction &setPa(REG const reg) VB_NOEXCEPT;

  ///
  /// @brief Set a register to the Db field
  ///
  /// @param reg Register to encode
  /// @return Instruction&
  Instruction &setDb(REG const reg) VB_NOEXCEPT;

  ///
  /// @brief Set an extended register to the Eb field
  ///
  /// @param reg Register to encode
  /// @return Instruction&
  Instruction &setEb(REG const reg) VB_NOEXCEPT;

  ///
  /// @brief Set a register to the Ab field
  ///
  /// @param reg Register to encode
  /// @return Instruction&
  Instruction &setAb(REG const reg) VB_NOEXCEPT;

  ///
  /// @brief Set a register to the Dc field
  ///
  /// @param reg Register to encode
  /// @return Instruction&
  Instruction &setDc(REG const reg) VB_NOEXCEPT;

  ///
  /// @brief Set an extended register to the Ec field
  ///
  /// @param reg Register to encode
  /// @return Instruction&
  Instruction &setEc(REG const reg) VB_NOEXCEPT;

  ///
  /// @brief Set a register to the Ac field
  ///
  /// @param reg Register to encode
  /// @return Instruction&
  Instruction &setAc(REG const reg) VB_NOEXCEPT;

  ///
  /// @brief Set a register to the Dd field
  ///
  /// @param reg Register to encode
  /// @return Instruction&
  Instruction &setDd(REG const reg) VB_NOEXCEPT;

  ///
  /// @brief Set an extended register to the Ed field
  ///
  /// @param reg Register to encode
  /// @return Instruction&
  Instruction &setEd(REG const reg) VB_NOEXCEPT;

  ///
  /// @brief Set a 5-bit value to the n field
  ///
  /// @param n Value to set to the n field
  /// @return Instruction&
  inline Instruction &setN(SafeUInt<5U> const n) VB_NOEXCEPT {
    return setN(n.value());
  }

  ///
  /// @brief Set a 2-bit value to the n scale field
  ///
  /// @param n Value to set to the n scale field
  /// @return Instruction&
  inline Instruction &setNSc(SafeUInt<2U> const n) VB_NOEXCEPT {
    return setNSc(n.value());
  }

  ///
  /// @brief Set a 5-bit value to the pos field
  ///
  /// @param pos Value to set to the pos field
  /// @return Instruction&
  inline Instruction &setPos(SafeUInt<5U> const pos) VB_NOEXCEPT {
    return setPos(pos.value());
  }

  ///
  /// @brief Set a 5-bit value to the pos1 field
  ///
  /// @param pos Value to set to the pos1 field
  /// @return Instruction&
  inline Instruction &setPos1(SafeUInt<5U> const pos) VB_NOEXCEPT {
    return setPos1(pos.value());
  }

  ///
  /// @brief Set a 5-bit value to the pos2 field
  ///
  /// @param pos Value to set to the pos2 field
  /// @return Instruction&
  inline Instruction &setPos2(SafeUInt<5U> const pos) VB_NOEXCEPT {
    return setPos2(pos.value());
  }

  ///
  /// @brief Set a 5-bit value to the width field
  ///
  /// @param width Value to set to the width field
  /// @return Instruction&
  inline Instruction &setWidth(SafeUInt<5U> const width) VB_NOEXCEPT {
    return setWidth(width.value());
  }

  ///
  /// @brief Set a signed constant to the const9 field
  ///
  /// @param constant Constant to encode, will be sign-extended
  /// @return Instruction&
  inline Instruction &setConst9sx(SafeInt<9U> const constant) VB_NOEXCEPT {
    return setConst9sx(constant.value());
  }

  ///
  /// @brief Set a constant to the const9 field
  ///
  /// @param constant Constant to encode, will be zero-extended
  /// @return Instruction&
  inline Instruction &setConst9zx(SafeUInt<9U> const constant) VB_NOEXCEPT {
    return setConst9zx(constant.value());
  }

  ///
  /// @brief Set an unsigned displacement to the const4zx field
  ///
  /// @param constant Constant to encode, will be zero-extended
  /// @return Instruction&
  inline Instruction &setConst4zx(SafeUInt<4U> const constant) VB_NOEXCEPT {
    return setConst4zx(constant.value());
  }

  ///
  /// @brief Set a signed displacement to the const4sx field
  ///
  /// @param constant Constant to encode, will be sign-extended
  /// @return Instruction&
  inline Instruction &setConst4sx(SafeInt<4U> const constant) VB_NOEXCEPT {
    return setConst4sx(constant.value());
  }

  ///
  /// @brief Set a constant to the const8zx4 field
  ///
  /// @param constant Constant to encode
  /// @return Instruction&
  inline Instruction &setConst8zx(SafeUInt<8U> const constant) VB_NOEXCEPT {
    return setConst8zx(constant.value());
  }

  ///
  /// @brief Set a constant to the const8zx4 field
  ///
  /// @param constant Constant to encode
  /// @return Instruction&
  inline Instruction &setConst8zxls2(SafeUInt<10U> const constant) VB_NOEXCEPT {
    return setConst8zxls2(constant.value());
  }

  ///
  /// @brief Set a constant to the const16 field
  ///
  /// @param constant Constant to encode
  /// @return Instruction&
  inline Instruction &setConst16(SafeUInt<16U> const constant) VB_NOEXCEPT {
    return setConst16(constant.value());
  }

  ///
  /// @brief Set a constant to the const16zx field
  ///
  /// @param constant Constant to encode
  /// @return Instruction&
  inline Instruction &setConst16zx(SafeUInt<16U> const constant) VB_NOEXCEPT {
    return setConst16zx(constant.value());
  }

  ///
  /// @brief Set a 16-bit sign-extended constant to the const16sx field
  ///
  /// @param constant Constant to encode, will be sign-extended
  /// @return Instruction&
  inline Instruction &setConst16sx(SafeInt<16U> const constant) VB_NOEXCEPT {
    return setConst16sx(constant.value());
  }

  ///
  /// @brief Set 4-bit zero-extended offset to the off4 field
  ///
  /// @param offset Offset to encode, will be zero-extended
  /// @return Instruction&
  inline Instruction &setOff4zx(SafeUInt<4U> const offset) VB_NOEXCEPT {
    return setOff4zx(offset.value());
  }

  ///
  /// @brief Set 5-bit value as 4-bit<<1 zero-extended offset to the off4 field
  ///
  /// @param offset Offset to encode, will be zero-extended
  /// @return Instruction&
  inline Instruction &setOff4zxls1(SafeUInt<5U> const offset) VB_NOEXCEPT {
    return setOff4zxls1(offset.value());
  }

  ///
  /// @brief Set 6-bit value as 4-bit<<2 zero-extended offset to the off4 field
  ///
  /// @param offset Offset to encode, will be zero-extended
  /// @return Instruction&
  inline Instruction &setOff4zxls2(SafeUInt<6U> const offset) VB_NOEXCEPT {
    return setOff4zxls2(offset.value());
  }

  ///
  /// @brief Set 4-bit value zero-extended offset to the off4sro field
  ///
  /// @param offset Offset to encode, will be zero-extended
  /// @return Instruction&
  inline Instruction &setOff4srozx(SafeUInt<4U> const offset) VB_NOEXCEPT {
    return setOff4srozx(offset.value());
  }

  ///
  /// @brief Set 5-bit value as 4bit<<1 zero-extended offset to the off4sro field
  ///
  /// @param offset Offset to encode, will be zero-extended
  /// @return Instruction&
  inline Instruction &setOff4srozxls1(SafeUInt<5U> const offset) VB_NOEXCEPT {
    return setOff4srozxls1(offset.value());
  }

  ///
  /// @brief Set 6-bit value as 4-bit<<2 zero-extended offset to the off4sro field
  ///
  /// @param offset Offset to encode, will be zero-extended
  /// @return Instruction&
  inline Instruction &setOff4srozxls2(SafeUInt<6U> const offset) VB_NOEXCEPT {
    return setOff4srozxls2(offset.value());
  }

  ///
  /// @brief Set 10-bit sign-extended offset to the off10 field
  ///
  /// @param offset Offset to encode, will be sign-extended
  /// @return Instruction&
  inline Instruction &setOff10sx(SafeInt<10> const offset) VB_NOEXCEPT {
    return setOff10sx(offset.value());
  }

  ///
  /// @brief Set 16-bit sign-extended offset to the off16sx field
  ///
  /// @param offset Offset to encode, will be sign-extended
  /// @return Instruction&
  inline Instruction &setOff16sx(SafeInt<16U> const offset) VB_NOEXCEPT {
    return setOff16sx(offset.value());
  }

  ///
  /// @brief Set a signed displacement to the disp4zx2 field
  ///
  /// @param disp Actual displacement to encode, must be 2-byte aligned
  /// @return Instruction&
  inline Instruction &setDisp4zx2(SafeUInt<5U> const disp) VB_NOEXCEPT {
    return setDisp4zx2(disp.value());
  }

  ///
  /// @brief Set a signed displacement to the disp15sx2 field
  ///
  /// @param disp Actual displacement to encode, must be 2-byte aligned
  /// @return Instruction&
  inline Instruction &setDisp15sx2(SafeInt<16U> const disp) VB_NOEXCEPT {
    return setDisp15sx2(disp.value());
  }

  ///
  /// @brief Set a signed displacement to the disp24sx2 field
  ///
  /// @param disp Actual displacement to encode, must be 2-byte aligned
  /// @return Instruction&
  inline Instruction &setDisp24sx2(SafeInt<25> const disp) VB_NOEXCEPT {
    return setDisp24sx2(disp.value());
  }

  ///
  /// @brief Check if a given absolute address can be encoded with setAbsDisp25sx2, i.e. it is 2-byte aligned and
  /// conforms to the mask 0xF01FFFFE
  ///
  /// @param addr Absolute address to encode
  /// @return bool
  inline static bool fitsAbsDisp24sx2(uint32_t const addr) VB_NOEXCEPT {
    return (addr & 0xF0'1F'FF'FEU) == addr;
  }

  ///
  /// @brief Set an absolute address to the disp24 field
  ///
  /// @param addr Absolute address to encode, must be 2-byte aligned and conform to the mask 0xF01FFFFE
  /// @return Instruction&
  Instruction &setAbsDisp24sx2(uint32_t const addr) VB_NOEXCEPT;

  ///
  /// @brief Read the 4-bit unsigned (scaled) displacement encoded in this branch instruction
  /// CAUTION: Undefined behavior if this instruction is no short relative-immediate branch instruction
  ///
  /// @return uint32_t The 4-bit unsigned (scaled) displacement
  uint32_t readDisp4zx2BranchOffset() const VB_NOEXCEPT;

  ///
  /// @brief Whether this branch instruction encodes a 15-bit (scaled) displacement
  /// CAUTION: Undefined behavior if this instruction is no relative-immediate branch instruction
  ///
  /// @return bool Whether this branch instruction encodes a 15-bit (scaled) displacement
  bool isDisp15x2BranchOffset() const VB_NOEXCEPT;

  ///
  /// @brief Read the 15-bit or 24-bit (scaled) displacement encoded in this branch instruction
  /// CAUTION: Undefined behavior if this instruction is no relative-immediate branch instruction
  ///
  /// @return int32_t The 15-bit or 24-bit (scaled) displacement
  int32_t readDisp15oDisp24x2BranchOffset() const VB_NOEXCEPT;

  ///
  /// @brief Write the instruction to the output binary
  ///
  void emitCode();

  ///
  /// @brief Write the instruction to the output binary and return a corresponding RelPatchObj
  /// Not valid for LEA
  ///
  /// @return Corresponding RelPatchObj
  inline RelPatchObj prepJmp() {
    RelPatchObj const relPatchObj{RelPatchObj(binary_.size(), binary_)};
    emitCode();
    return relPatchObj;
  }

  ///
  /// @brief Write the instruction to the output binary and return a corresponding RelPatchObj
  /// Only valid for LEA
  ///
  /// @return Corresponding RelPatchObj
  inline RelPatchObj prepLEA() {
    RelPatchObj const relPatchObj{RelPatchObj(binary_.size(), binary_, false)};
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
  inline OPCodeTemplate const &getOPCode() const VB_NOEXCEPT {
    return opcode_;
  }

  ///
  /// @brief
  ///
  /// @param value
  /// @return uint32_t
  ///
  static inline SafeInt<16U> lower16sx(uint32_t const value) VB_NOEXCEPT {
    return SafeInt<16U>::fromAny(bit_cast<int16_t>(static_cast<uint16_t>(value)));
  }

private:
  ///
  /// @brief Set a 5-bit value to the pos field
  ///
  /// @param pos Value to set to the pos field
  /// @return Instruction&
  Instruction &setPos(uint32_t const pos) VB_NOEXCEPT;

  ///
  /// @brief Set a 5-bit value to the pos1 field
  ///
  /// @param pos Value to set to the pos1 field
  /// @return Instruction&
  Instruction &setPos1(uint32_t const pos) VB_NOEXCEPT;

  ///
  /// @brief Set a 5-bit value to the pos2 field
  ///
  /// @param pos Value to set to the pos2 field
  /// @return Instruction&
  Instruction &setPos2(uint32_t const pos) VB_NOEXCEPT;

  ///
  /// @brief Set a 5-bit value to the n field
  ///
  /// @param n Value to set to the n field
  /// @return Instruction&
  Instruction &setN(uint32_t const n) VB_NOEXCEPT;

  ///
  /// @brief Set a 2-bit value to the n scale field
  ///
  /// @param n Value to set to the n scale field
  /// @return Instruction&
  Instruction &setNSc(uint32_t const n) VB_NOEXCEPT;

  ///
  /// @brief Set a constant to the const9 field
  ///
  /// @param constant Constant to encode
  /// @return Instruction&
  Instruction &setConst9(uint32_t const constant) VB_NOEXCEPT;

  ///
  /// @brief Set a 5-bit value to the width field
  ///
  /// @param width Value to set to the width field
  /// @return Instruction&
  Instruction &setWidth(uint32_t const width) VB_NOEXCEPT;

  ///
  /// @brief Set a signed constant to the const9 field
  ///
  /// @param constant Constant to encode, will be sign-extended
  /// @return Instruction&
  Instruction &setConst9sx(int32_t const constant) VB_NOEXCEPT;

  ///
  /// @brief Set a constant to the const9 field
  ///
  /// @param constant Constant to encode, will be zero-extended
  /// @return Instruction&
  Instruction &setConst9zx(uint32_t const constant) VB_NOEXCEPT;

  ///
  /// @brief Set an unsigned displacement to the const4zx field
  ///
  /// @param constant Constant to encode, will be zero-extended
  /// @return Instruction&
  Instruction &setConst4zx(uint32_t const constant) VB_NOEXCEPT;

  ///
  /// @brief Set a signed displacement to the const4sx field
  ///
  /// @param constant Constant to encode, will be sign-extended
  /// @return Instruction&
  Instruction &setConst4sx(int32_t const constant) VB_NOEXCEPT;

  ///
  /// @brief Set a constant to the const8zx4 field
  ///
  /// @param constant Constant to encode
  /// @return Instruction&
  Instruction &setConst8zx(uint32_t const constant) VB_NOEXCEPT;

  ///
  /// @brief Set a constant to the const16 field
  ///
  /// @param constant Constant to encode
  /// @return Instruction&
  Instruction &setConst16(uint32_t const constant) VB_NOEXCEPT;

  ///
  /// @brief Set a constant to the const16zx field
  ///
  /// @param constant Constant to encode
  /// @return Instruction&
  Instruction &setConst16zx(uint32_t const constant) VB_NOEXCEPT;

  ///
  /// @brief Set a constant to the const8zx4 field
  ///
  /// @param constant Constant to encode
  /// @return Instruction&
  Instruction &setConst8zxls2(uint32_t const constant) VB_NOEXCEPT;

  ///
  /// @brief Set a 16-bit sign-extended constant to the const16sx field
  ///
  /// @param constant Constant to encode, will be sign-extended
  /// @return Instruction&
  Instruction &setConst16sx(int32_t const constant) VB_NOEXCEPT;

  ///
  /// @brief Set 4-bit zero-extended offset to the off4 field
  ///
  /// @param offset Offset to encode, will be zero-extended
  /// @return Instruction&
  Instruction &setOff4zx(uint32_t const offset) VB_NOEXCEPT;

  ///
  /// @brief Set 5-bit value as 4-bit<<1 zero-extended offset to the off4 field
  ///
  /// @param offset Offset to encode, will be zero-extended
  /// @return Instruction&
  Instruction &setOff4zxls1(uint32_t const offset) VB_NOEXCEPT;

  ///
  /// @brief Set 6-bit value as 4-bit<<2 zero-extended offset to the off4 field
  ///
  /// @param offset Offset to encode, will be zero-extended
  /// @return Instruction&
  Instruction &setOff4zxls2(uint32_t const offset) VB_NOEXCEPT;

  ///
  /// @brief Set 4-bit zero-extended offset to the off4sro field
  ///
  /// @param offset Offset to encode, will be zero-extended
  /// @return Instruction&
  Instruction &setOff4srozx(uint32_t const offset) VB_NOEXCEPT;

  ///
  /// @brief Set 5-bit value as 4-bit<<1 zero-extended offset to the off4sro field
  ///
  /// @param offset Offset to encode, will be zero-extended
  /// @return Instruction&
  Instruction &setOff4srozxls1(uint32_t const offset) VB_NOEXCEPT;

  ///
  /// @brief Set 6-bit value as 4-bit<<2 zero-extended offset to the off4sro field
  ///
  /// @param offset Offset to encode, will be zero-extended
  /// @return Instruction&
  Instruction &setOff4srozxls2(uint32_t const offset) VB_NOEXCEPT;

  ///
  /// @brief Set 10-bit sign-extended offset to the off10 field
  ///
  /// @param offset Offset to encode, will be sign-extended
  /// @return Instruction&
  Instruction &setOff10sx(int32_t const offset) VB_NOEXCEPT;

  ///
  /// @brief Set 16-bit sign-extended offset to the off16sx field
  ///
  /// @param offset Offset to encode, will be sign-extended
  /// @return Instruction&
  Instruction &setOff16sx(int32_t const offset) VB_NOEXCEPT;

  ///
  /// @brief Set a signed displacement to the disp4zx2 field
  ///
  /// @param disp Actual displacement to encode, must be 2-byte aligned
  /// @return Instruction&
  Instruction &setDisp4zx2(uint32_t const disp) VB_NOEXCEPT;

  ///
  /// @brief Set a signed displacement to the disp15sx2 field
  ///
  /// @param disp Actual displacement to encode, must be 2-byte aligned
  /// @return Instruction&
  Instruction &setDisp15sx2(int32_t const disp) VB_NOEXCEPT;

  ///
  /// @brief Set a signed displacement to the disp24sx2 field
  ///
  /// @param disp Actual displacement to encode, must be 2-byte aligned
  /// @return Instruction&
  Instruction &setDisp24sx2(int32_t const disp) VB_NOEXCEPT;

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

} // namespace tc
} // namespace vb

#endif

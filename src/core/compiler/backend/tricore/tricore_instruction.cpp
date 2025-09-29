///
/// @file tricore_instruction.cpp
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
// coverity[autosar_cpp14_a16_2_2_violation]
#include "src/config.hpp"

#ifdef JIT_TARGET_TRICORE
#include <cassert>
#include <cstdint>

#include "tricore_assembler.hpp"
#include "tricore_instruction.hpp"

#include "src/core/common/util.hpp"
#include "src/core/compiler/backend/tricore/tricore_encoding.hpp"
#include "src/core/compiler/common/MemWriter.hpp"
#include "src/core/compiler/common/OPCode.hpp"

namespace vb {
namespace tc {

Instruction::Instruction(OPCodeTemplate const opcode, MemWriter &binary) VB_NOEXCEPT : opcode_{opcode}, binary_(binary), emitted_(false) {
}

void Instruction::emitCode() {
  assert(!emitted_ && "Instruction can only be emitted once");
  // Set this to true before we try to write, otherwise the instruction might be destroyed without it being set to true
  // in an out of memory situation (exception from binary_.write<...>(...))
  emitted_ = true;
  if (is16BitInstr(opcode_)) {
    assert((opcode_ >> 16U == 0U) && "High bytes of 16b instruction not empty");
    binary_.write<uint16_t>(static_cast<uint16_t>(opcode_));
  } else {
    binary_.write<OPCodeTemplate>(opcode_);
  }
}

Instruction &Instruction::setDa(REG const reg) VB_NOEXCEPT {
  assert((reg != REG::NONE) && "Invalid register");
  assert(RegUtil::isDATA(reg) && "Only supports data registers");

  opcode_ |= (static_cast<uint32_t>(reg) & 0xFU) << 8U;
  return *this;
}

Instruction &Instruction::setEa(REG const reg) VB_NOEXCEPT {
  assert((reg != REG::NONE) && "Invalid register");
  assert(RegUtil::isDATA(reg) && "Only supports data registers");
  assert(RegUtil::canBeExtReg(reg) && "Register not usable as extended register");

  return setDa(reg);
}

Instruction &Instruction::setAa(REG const reg) VB_NOEXCEPT {
  assert((reg != REG::NONE) && "Invalid register");
  assert(!RegUtil::isDATA(reg) && "Only supports address registers");

  opcode_ |= (static_cast<uint32_t>(reg) & 0xFU) << 8U;
  return *this;
}

Instruction &Instruction::setPa(REG const reg) VB_NOEXCEPT {
  assert((reg != REG::NONE) && "Invalid register");
  assert(!RegUtil::isDATA(reg) && "Only supports address registers");
  assert(RegUtil::canBeExtReg(reg) && "Register not usable as extended register");

  opcode_ |= (static_cast<uint32_t>(reg) & 0xFU) << 8U;
  return *this;
}

Instruction &Instruction::setDb(REG const reg) VB_NOEXCEPT {
  assert((reg != REG::NONE) && "Invalid register");
  assert(RegUtil::isDATA(reg) && "Only supports data registers");

  opcode_ |= (static_cast<uint32_t>(reg) & 0xFU) << 12U;
  return *this;
}

Instruction &Instruction::setEb(REG const reg) VB_NOEXCEPT {
  assert((reg != REG::NONE) && "Invalid register");
  assert(RegUtil::isDATA(reg) && "Only supports data registers");
  assert(RegUtil::canBeExtReg(reg) && "Register not usable as extended register");

  return setDb(reg);
}

Instruction &Instruction::setAb(REG const reg) VB_NOEXCEPT {
  assert((reg != REG::NONE) && "Invalid register");
  assert(!RegUtil::isDATA(reg) && "Only supports address registers");

  opcode_ |= (static_cast<uint32_t>(reg) & 0xFU) << 12U;
  return *this;
}

Instruction &Instruction::setDc(REG const reg) VB_NOEXCEPT {
  assert((reg != REG::NONE) && "Invalid register");
  assert(RegUtil::isDATA(reg) && "Only supports data registers");

  uint32_t const lsMask{is16BitInstr(opcode_) ? 8_U32 : 28_U32};
  opcode_ |= (static_cast<uint32_t>(reg) & 0xFU) << lsMask;
  return *this;
}

Instruction &Instruction::setEc(REG const reg) VB_NOEXCEPT {
  assert((reg != REG::NONE) && "Invalid register");
  assert(RegUtil::isDATA(reg) && "Only supports data registers");
  assert(RegUtil::canBeExtReg(reg) && "Register not usable as extended register");

  return setDc(reg);
}

Instruction &Instruction::setAc(REG const reg) VB_NOEXCEPT {
  assert((reg != REG::NONE) && "Invalid register");
  assert(!RegUtil::isDATA(reg) && "Only supports address registers");

  uint32_t const lsMask{is16BitInstr(opcode_) ? 8_U32 : 28_U32};
  opcode_ |= (static_cast<uint32_t>(reg) & 0xFU) << lsMask;
  return *this;
}

Instruction &Instruction::setDd(REG const reg) VB_NOEXCEPT {
  assert((reg != REG::NONE) && "Invalid register");
  assert(RegUtil::isDATA(reg) && "Only supports data registers");

  opcode_ |= (static_cast<uint32_t>(reg) & 0xFU) << 24U;
  return *this;
}

Instruction &Instruction::setEd(REG const reg) VB_NOEXCEPT {
  assert((reg != REG::NONE) && "Invalid register");
  assert(RegUtil::isDATA(reg) && "Only supports data registers");
  assert(RegUtil::canBeExtReg(reg) && "Register not usable as extended register");

  return setDd(reg);
}

Instruction &Instruction::setN(uint32_t const n) VB_NOEXCEPT {
  assert(in_range<5U>(n) && "Value not in range");

  opcode_ |= (n & 0xFU) << 12U;
  opcode_ |= ((n & 0x10U) >> 4U) << 7U;
  return *this;
}

Instruction &Instruction::setNSc(uint32_t const n) VB_NOEXCEPT {
  assert(in_range<2U>(n) && "Value not in range");

  opcode_ |= (n & 0b11U) << 16U;
  return *this;
}

Instruction &Instruction::setPos(uint32_t const pos) VB_NOEXCEPT {
  assert(in_range<5U>(pos) && "Value not in range");

  opcode_ |= static_cast<uint32_t>(pos) << 23U;
  return *this;
}

Instruction &Instruction::setPos1(uint32_t const pos) VB_NOEXCEPT {
  assert(in_range<5U>(pos) && "Value not in range");

  opcode_ |= static_cast<uint32_t>(pos) << 16U;
  return *this;
}

Instruction &Instruction::setPos2(uint32_t const pos) VB_NOEXCEPT {
  assert(in_range<5U>(pos) && "Value not in range");

  opcode_ |= static_cast<uint32_t>(pos) << 23U;
  return *this;
}

Instruction &Instruction::setWidth(uint32_t const width) VB_NOEXCEPT {
  assert(in_range<5U>(width) && "Value not in range");

  opcode_ |= static_cast<uint32_t>(width) << 16U;
  return *this;
}

Instruction &Instruction::setConst9(uint32_t const constant) VB_NOEXCEPT {
  assert(in_range<9U>(constant) && "Value not in range");

  opcode_ |= constant << 12U;
  return *this;
}

Instruction &Instruction::setConst9sx(int32_t const constant) VB_NOEXCEPT {
  assert(in_range<9>(constant) && "Value not in range");
  return setConst9(bit_cast<uint32_t>(constant) & 0x1FFU); // TODO(clean up &)
}

Instruction &Instruction::setConst9zx(uint32_t const constant) VB_NOEXCEPT {
  return setConst9(constant);
}

Instruction &Instruction::setConst4zx(uint32_t const constant) VB_NOEXCEPT {
  assert(in_range<4>(constant) && "Value not in range");
  opcode_ |= (constant & 0xFU) << 12U;
  return *this;
}

Instruction &Instruction::setConst4sx(int32_t const constant) VB_NOEXCEPT {
  assert(in_range<4>(constant) && "Value not in range");
  return setConst4zx(bit_cast<uint32_t>(constant) & 0xFU);
}

Instruction &Instruction::setConst8zx(uint32_t const constant) VB_NOEXCEPT {
  assert(in_range<8>(constant) && "Value not in range");
  opcode_ |= (constant & 0b1111'1111U) << 8U;
  return *this;
}

Instruction &Instruction::setConst8zxls2(uint32_t const constant) VB_NOEXCEPT {
  assert(in_range<8>(constant >> 2U) && ((constant & 0b11U) == 0U) && "Value not in range");
  opcode_ |= ((constant >> 2U) & 0b1111'1111U) << 8U;
  return *this;
}

Instruction &Instruction::setConst16(uint32_t const constant) VB_NOEXCEPT {
  assert(in_range<16>(constant) && "Constant out of range");

  opcode_ |= constant << 12U;
  return *this;
}

Instruction &Instruction::setConst16zx(uint32_t const constant) VB_NOEXCEPT {
  return setConst16(constant);
}

Instruction &Instruction::setConst16sx(int32_t const constant) VB_NOEXCEPT {
  assert(in_range<16>(constant) && "Constant out of range");

  uint32_t const convConst{bit_cast<uint32_t>(constant) & 0xFFFFU};
  opcode_ |= convConst << 12U;
  return *this;
}

Instruction &Instruction::setOff4zx(uint32_t const offset) VB_NOEXCEPT {
  assert(in_range<4>(offset) && "Offset out of range");

  opcode_ |= (offset & 0b1111U) << 12U;
  return *this;
}
Instruction &Instruction::setOff4zxls1(uint32_t const offset) VB_NOEXCEPT {
  assert(in_range<4>(offset >> 1U) && ((offset & 0b1U) == 0U) && "Offset out of range");

  opcode_ |= ((offset >> 1U) & 0b1111U) << 12U;
  return *this;
}
Instruction &Instruction::setOff4zxls2(uint32_t const offset) VB_NOEXCEPT {
  assert(in_range<4>(offset >> 2U) && ((offset & 0b11U) == 0U) && "Offset out of range");

  opcode_ |= ((offset >> 2U) & 0b1111U) << 12U;
  return *this;
}

Instruction &Instruction::setOff4srozx(uint32_t const offset) VB_NOEXCEPT {
  assert(in_range<4>(offset) && "Offset out of range");

  opcode_ |= (offset & 0b1111U) << 8U;
  return *this;
}
Instruction &Instruction::setOff4srozxls1(uint32_t const offset) VB_NOEXCEPT {
  assert(in_range<4>(offset >> 1U) && ((offset & 0b1U) == 0U) && "Offset out of range");

  opcode_ |= ((offset >> 1U) & 0b1111U) << 8U;
  return *this;
}
Instruction &Instruction::setOff4srozxls2(uint32_t const offset) VB_NOEXCEPT {
  assert(in_range<4>(offset >> 2U) && ((offset & 0b11U) == 0U) && "Offset out of range");

  opcode_ |= ((offset >> 2U) & 0b1111U) << 8U;
  return *this;
}

Instruction &Instruction::setOff10sx(int32_t const offset) VB_NOEXCEPT {
  assert(in_range<10>(offset) && "Offset out of range");

  uint32_t const convOff{bit_cast<uint32_t>(offset)};
  opcode_ |= (convOff & 0x3FU) << 16U;
  opcode_ |= ((convOff >> 6U) & 0xFU) << 28U;
  return *this;
}

Instruction &Instruction::setOff16sx(int32_t const offset) VB_NOEXCEPT {
  assert(in_range<16>(offset) && "Offset out of range");

  uint32_t const convOff{bit_cast<uint32_t>(offset)};
  opcode_ |= (convOff & 0x3FU) << 16U;
  opcode_ |= ((convOff >> 6U) & 0xFU) << 28U;
  opcode_ |= ((convOff >> 10U) & 0x3FU) << 22U;
  return *this;
}

Instruction &Instruction::setDisp4zx2(uint32_t const disp) VB_NOEXCEPT {
  assert(((disp & 0x1U) == 0U) && "Displacement not aligned to 2-byte boundary");
  assert((in_range<5>(disp)) && "Displacement too large");

  uint32_t const reducedDisp{disp >> 1U};
  opcode_ |= (reducedDisp & 0xFU) << 8U;

  return *this;
}

Instruction &Instruction::setDisp15sx2(int32_t const disp) VB_NOEXCEPT {
  assert(((bit_cast<uint32_t>(disp) & 0x1U) == 0U) && "Displacement not aligned to 2-byte boundary");
  assert((in_range<16>(disp)) && "Displacement too large");

  uint32_t const reducedDisp{bit_cast<uint32_t>(disp) >> 1U};
  opcode_ &= ~(0x7FFF_U32 << 16_U32);
  opcode_ |= (reducedDisp & 0x7FFFU) << 16U;

  return *this;
}

Instruction &Instruction::setDisp24sx2(int32_t const disp) VB_NOEXCEPT {
  assert(((bit_cast<uint32_t>(disp) & 0x1U) == 0U) && "Displacement not aligned to 2-byte boundary");
  assert((in_range<25>(disp)) && "Displacement too large");

  uint32_t const reducedDisp{bit_cast<uint32_t>(disp) >> 1U};
  opcode_ &= 0xFFU;
  opcode_ |= (reducedDisp & 0xFFFFU) << 16U;
  opcode_ |= ((reducedDisp & 0xFF0000U) >> 16U) << 8U;

  return *this;
}

Instruction &Instruction::setAbsDisp24sx2(uint32_t const addr) VB_NOEXCEPT {
  assert(Instruction::fitsAbsDisp24sx2(addr) && "Absolute address cannot be represented");

  uint32_t const normalizedDisp{(((addr & 0xF0'00'00'00U) >> 28U) << 20U) | ((addr & 0x00'1F'FF'FEU) >> 1U)};
  opcode_ |= (normalizedDisp & 0xFFFFU) << 16U;
  opcode_ |= ((normalizedDisp & 0xFF0000U) >> 16U) << 8U;

  return *this;
}

bool Instruction::isDisp15x2BranchOffset() const VB_NOEXCEPT {
  return (((opcode_ & 0xFU) == 0xFU) || ((opcode_ & 0xFFU) == 0xBDU)) || (((opcode_ & 0xFFU) == 0x7DU) || ((opcode_ & 0xFFU) == 0xFDU));
}

uint32_t Instruction::readDisp4zx2BranchOffset() const VB_NOEXCEPT {
  assert(is16BitInstr(opcode_) && "Can only be used for 16-bit instructions");

  uint32_t const offset{(opcode_ >> 8U) & 0xFU};
  return offset << 1U;
}

int32_t Instruction::readDisp15oDisp24x2BranchOffset() const VB_NOEXCEPT {
  uint32_t const branchWidth{isDisp15x2BranchOffset() ? 15_U32 : 24_U32};
  uint32_t offset{0U};
  if (branchWidth == 15U) {
    offset = (opcode_ >> 16U) & 0x7FFFU;
  } else {
    offset = (opcode_ >> 16U) & 0xFFFFU;
    offset |= ((opcode_ >> 8U) & 0xFFU) << 16U;
  }

  if (((offset >> (branchWidth - 1U)) & 0x1U) != 0U) {
    offset |= ~((1_U32 << branchWidth) - 1U); // Sign extend
  }

  return bit_cast<int32_t>(offset << 1U);
}

} // namespace tc
} // namespace vb

#endif

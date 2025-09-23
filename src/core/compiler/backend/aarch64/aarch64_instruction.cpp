///
/// @file aarch64_instruction.cpp
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
#ifdef JIT_TARGET_AARCH64
#include <cassert>
#include <cstdint>

#include "aarch64_aux.hpp"
#include "aarch64_instruction.hpp"

#include "src/core/common/util.hpp"
#include "src/core/compiler/backend/aarch64/aarch64_encoding.hpp"
#include "src/core/compiler/common/MemWriter.hpp"

namespace vb {
namespace aarch64 {

Instruction::Instruction(OPCodeTemplate const opcode, MemWriter &binary) VB_NOEXCEPT : opcode_{opcode}, binary_(binary), emitted_(false) {
}
Instruction::Instruction(AbstrInstr const abstrInstr, MemWriter &binary) VB_NOEXCEPT : opcode_{abstrInstr.opcode}, binary_(binary), emitted_(false) {
}
Instruction::~Instruction() VB_NOEXCEPT {
  assert(emitted_ && "Instruction was created, but has not been emitted");
}

Instruction &Instruction::setImm6(uint32_t const imm6) VB_NOEXCEPT {
  opcode_ |= imm6 << 10U;
  return *this;
}

// LSR = UBFM <Wd>, <Wn>, #<shift>, #31 OR // LSR = UBFM <Wd>, <Wn>, #<shift>,
// #63 ASR = SBFM <Wd>, <Wn>, #<shift>, #31 OR // ASR = SBFM <Wd>, <Wn>,
// #<shift>, #63 LSL = UBFM <Wd>, <Wn>, #(-<shift> MOD 32), #(31-<shift>) OR //
// LSL = UBFM <Wd>, <Wn>, #(-<shift> MOD 64), #(63-<shift>)
Instruction &Instruction::setImm6x(bool const left, uint32_t const imm6x) VB_NOEXCEPT {
  uint32_t const sfBit{(opcode_ >> 31U) & 0b1U};
  bool const is64{sfBit == 0b1U};

  uint32_t const imm6Mask{is64 ? 0b0011'1111_U32 : 0b0001'1111_U32};

  uint32_t const immr{left ? ((0U - imm6x) & imm6Mask) : imm6x};
  uint32_t const imms{left ? (imm6Mask - imm6x) : imm6Mask};
  opcode_ |= imms << 10U;
  opcode_ |= immr << 16U;
  return *this;
}

Instruction &Instruction::setSImm7ls4(int32_t const imm) VB_NOEXCEPT {
  assert((static_cast<uint32_t>(imm) & 0b1111U) == 0U);
  int32_t const reducedImm{imm / 16};

  opcode_ |= (bit_cast<uint32_t>(reducedImm) & 0x7FU) << 15U;
  return *this;
}

Instruction &Instruction::setSImm7ls3(int32_t const imm) VB_NOEXCEPT {
  assert((static_cast<uint32_t>(imm) & 0b111U) == 0U);
  int32_t const reducedImm{imm / 8};

  opcode_ |= (bit_cast<uint32_t>(reducedImm) & 0x7FU) << 15U;
  return *this;
}

Instruction &Instruction::setSImm7ls2(int32_t const imm) VB_NOEXCEPT {
  assert((static_cast<uint32_t>(imm) & 0b11U) == 0U);
  int32_t const reducedImm{imm / 4};

  opcode_ |= (bit_cast<uint32_t>(reducedImm) & 0x7FU) << 15U;
  return *this;
}

Instruction &Instruction::setT(REG const reg) VB_NOEXCEPT {
  return setD(reg);
}
Instruction &Instruction::setT1(REG const reg) VB_NOEXCEPT {
  return setD(reg);
}
Instruction &Instruction::setD(REG const reg) VB_NOEXCEPT {
  opcode_ |= static_cast<uint32_t>(reg) & 0b1'1111U;
  return *this;
}
Instruction &Instruction::setN(REG const reg) VB_NOEXCEPT {
  opcode_ |= (static_cast<uint32_t>(reg) & 0b1'1111U) << 5U;
  return *this;
}
Instruction &Instruction::clearN() VB_NOEXCEPT {
  opcode_ &= ~((0b1'1111_U32) << 5_U32);
  return *this;
}
REG Instruction::getN() const VB_NOEXCEPT {
  uint32_t const rawReg{(opcode_ >> 5U) & 0b1'1111U};
  return static_cast<REG>(rawReg);
}
Instruction &Instruction::setM(REG const reg) VB_NOEXCEPT {
  opcode_ |= (static_cast<uint32_t>(reg) & 0b1'1111U) << 16U;
  return *this;
}
Instruction &Instruction::setA(REG const reg) VB_NOEXCEPT {
  opcode_ |= (static_cast<uint32_t>(reg) & 0b1'1111U) << 10U;
  return *this;
}
Instruction &Instruction::setT2(REG const reg) VB_NOEXCEPT {
  return setA(reg);
}
Instruction &Instruction::setImm12zx(uint32_t const imm) VB_NOEXCEPT {
  opcode_ |= (imm & 0xFFFU) << 10U;

  return *this;
}

Instruction &Instruction::setImm12zxls12(uint32_t const imm) VB_NOEXCEPT {
  opcode_ |= ((imm >> 12U) & 0xFFFU) << 10U;
  opcode_ |= 1_U32 << 22_U32;
  return *this;
}

Instruction &Instruction::setImmBitmask(uint64_t const imm) VB_NOEXCEPT {
  uint32_t const sfBit{(opcode_ >> 31U) & 0b1U};
  bool const is64{sfBit == 0b1U};

  uint64_t encoding{0U};
  bool const valid{processLogicalImmediate(imm, is64, encoding)};
  static_cast<void>(valid);
  assert(valid);
  opcode_ |= static_cast<uint32_t>(encoding & 0b1'1111'1111'1111U) << 10U;
  return *this;
}
Instruction &Instruction::setRawImmBitmask(uint32_t const encoding) VB_NOEXCEPT {
  opcode_ |= (encoding & 0b1'1111'1111'1111U) << 10U;
  return *this;
}

// Only for MOV
Instruction &Instruction::setImm16Ols(uint32_t const imm, uint32_t const shift) VB_NOEXCEPT {
  assert(shift <= 48 && shift % 16 == 0 && "Shift must be less or equal than 48 and a multiple of 16");
  assert(imm <= UINT16_MAX && "Immediate bigger than UINT16_MAX");
  opcode_ |= ((shift / 16U) & 0b11U) << 21U;
  opcode_ |= (imm & 0xFFFFU) << 5U;
  return *this;
}
Instruction &Instruction::setRawFMOVImm8(uint32_t const rawFloatImm) VB_NOEXCEPT {
  assert(rawFloatImm <= UINT8_MAX && "Float immediate out of range");
  opcode_ |= (rawFloatImm & 0xFFU) << 13U;
  return *this;
}
Instruction &Instruction::setCond(bool const lowCond, CC const cc) VB_NOEXCEPT {
  uint32_t const offset{(lowCond ? 0_U32 : 12_U32)};
  opcode_ |= (static_cast<uint32_t>(cc) & 0x1FU) << offset;
  return *this;
}

Instruction &Instruction::setImm12zxls1(uint32_t const imm) VB_NOEXCEPT {
  assert((imm & 1U) == 0U);
  uint32_t const reducedImm{imm >> 1U};
  opcode_ |= reducedImm << 10U;
  return *this;
}

Instruction &Instruction::setImm12zxls2(uint32_t const imm) VB_NOEXCEPT {
  assert((imm & 0b11U) == 0U);
  uint32_t const reducedImm{imm >> 2U};
  opcode_ |= reducedImm << 10U;
  return *this;
}

Instruction &Instruction::setImm12zxls3(uint32_t const imm) VB_NOEXCEPT {
  assert((imm & 0b111U) == 0U);
  uint32_t const reducedImm{imm >> 3U};
  opcode_ |= reducedImm << 10U;
  return *this;
}

// Load/Store Offset
Instruction &Instruction::setUnscSImm9(int32_t const imm) VB_NOEXCEPT {
  assert(imm >= -256 && imm <= 255 && "Immediate out of range");
  opcode_ |= (bit_cast<uint32_t>(imm) & 0x1FFU) << 12U;
  return *this;
}

Instruction &Instruction::setImm19ls2BranchOffset(int32_t const offset) VB_NOEXCEPT {
  constexpr uint32_t branchWidth{19U};
  assert(offset <= static_cast<int32_t>((1U << (branchWidth + 1U)) - 1U) && offset >= -static_cast<int32_t>(1U << (branchWidth + 1U)) &&
         "Branch offset not in range");
  assert(offset % 4 == 0 && "Branch offset targeting unaligned address");

  opcode_ &= ~(((static_cast<uint32_t>(1U) << branchWidth) - static_cast<uint32_t>(1U)) << 5U);
  opcode_ |= ((bit_cast<uint32_t>(offset) >> 2U) & ((1_U32 << branchWidth) - 1U)) << 5U;
  return *this;
}

Instruction &Instruction::setImm26ls2BranchOffset(int32_t const offset) VB_NOEXCEPT {
  constexpr uint32_t branchWidth{26U};
  assert(offset <= static_cast<int32_t>((1U << (branchWidth + 1U)) - 1U) && offset >= -static_cast<int32_t>(1U << (branchWidth + 1U)) &&
         "Branch offset not in range");
  assert(offset % 4 == 0 && "Branch offset targeting unaligned address");

  opcode_ &= ~((1_U32 << branchWidth) - 1U);
  opcode_ |= (bit_cast<uint32_t>(offset) >> 2U) & ((1_U32 << branchWidth) - 1U);
  return *this;
}

Instruction &Instruction::setOlsImm6(uint32_t const count) VB_NOEXCEPT {
  uint32_t const sfBit{(opcode_ >> 31U) & 0b1U};
  static_cast<void>(sfBit);
  assert(count <= (sfBit == 0b1U ? 63U : 31U) && "Shift count out of range");
  opcode_ |= (count & 0x3FU) << 10U;
  return *this;
}

Instruction &Instruction::setSigned21AddressOffset(int32_t const offset) VB_NOEXCEPT {
  assert(offset >= -(static_cast<int32_t>(1U << 20U) - 1) && offset <= static_cast<int32_t>(1U << 20U));
  opcode_ |= ((bit_cast<uint32_t>(offset) >> 2U) & 0x7FFFFU) << 5U;
  opcode_ |= (bit_cast<uint32_t>(offset) & 0b11U) << 29U;
  return *this;
}
bool Instruction::isImm19ls2BranchOffset() const VB_NOEXCEPT {
  return ((opcode_ >> 29U) & 0b11U) != 0U;
}
int32_t Instruction::readImm19o26ls2BranchOffset() const VB_NOEXCEPT {
  uint32_t const branchWidth{isImm19ls2BranchOffset() ? 19_U32 : 26_U32};
  uint32_t offset{opcode_ >> ((branchWidth == 19U) ? 5U : 0U)};
  if (((offset >> (branchWidth - 1U)) & 0x1U) != 0U) {
    offset |= ~((1_U32 << branchWidth) - 1U); // Sign extend
  } else {
    offset &= (1_U32 << branchWidth) - 1U; // Zero extend
  }
  return bit_cast<int32_t>(offset << 2U);
}

void Instruction::emitCode() {
  assert(!emitted_ && "Instruction can only be emitted once");
  // Set this to true before we try to write, otherwise the instruction might be destroyed without it being set to true
  // in an out of memory situation (exception from binary_.write<...>(...))
  emitted_ = true;
  binary_.write<OPCodeTemplate>(opcode_);
}

} // namespace aarch64
} // namespace vb
#endif

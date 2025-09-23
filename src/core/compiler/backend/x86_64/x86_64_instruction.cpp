///
/// @file x86_64_instruction.cpp
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

#ifdef JIT_TARGET_X86_64
#include <cassert>
#include <cstdint>

#include "x86_64_instruction.hpp"

#include "src/core/common/util.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_encoding.hpp"
#include "src/core/compiler/common/MemWriter.hpp"

namespace vb {
namespace x86_64 {

Instruction::Instruction(OPCodeTemplate const opcode, MemWriter &binary) VB_NOEXCEPT : opcode_{opcode},
                                                                                       cc_(CC::NONE),
                                                                                       rmType_(RMType::NONE),
                                                                                       immType_(ImmType::NONE),
                                                                                       immediate_(0U),
                                                                                       rReg_(REG::NONE),
                                                                                       rmBaseReg_(REG::NONE),
                                                                                       rmIndexReg_(REG::NONE),
                                                                                       rmIndexScalePow2_(0U),
                                                                                       rmDisplacement_(0),
                                                                                       binary_(binary),
                                                                                       emitted_(false) {
}
Instruction::Instruction(AbstrInstr const &abstrInstr, MemWriter &binary) VB_NOEXCEPT : opcode_{abstrInstr.opTemplate},
                                                                                        cc_(CC::NONE),
                                                                                        rmType_(RMType::NONE),
                                                                                        immType_(ImmType::NONE),
                                                                                        immediate_(0U),
                                                                                        rReg_(REG::NONE),
                                                                                        rmBaseReg_(REG::NONE),
                                                                                        rmIndexReg_(REG::NONE),
                                                                                        rmIndexScalePow2_(0U),
                                                                                        rmDisplacement_(0),
                                                                                        binary_(binary),
                                                                                        emitted_(false) {
}
Instruction::~Instruction() VB_NOEXCEPT {
  assert(emitted_ && "Instruction was created, but has not been emitted");
}

// Sets a register for the REG field in the MOD R/M byte.
// For RADD (Register add to opcode) extension as in MOV rax, imm64 or
// PUSH/POP, we set it to the rmBaseReg since it mandates the REX.B flag to be
// set instead of the REX.R flag
Instruction &Instruction::setR(REG const reg) VB_NOEXCEPT {
  if (opcode_.extension == OPCodeExt::RADD) {
    rmBaseReg_ = reg;
  } else {
    rReg_ = reg;
  }
  return *this;
}
// "Register for RM"
// Set the rm field of an instruction to direct register access (no
// dereference). e.g.: ADD r32, rm32 -> ADD rax, rbx
Instruction &Instruction::setR4RM(REG const reg) VB_NOEXCEPT {
  rmType_ = RMType::REG;
  rmBaseReg_ = reg;
  return *this;
}

Instruction &Instruction::setR8_4RM(REG const reg) VB_NOEXCEPT {
  rmType_ = RMType::REG;
  rmBaseReg_ = reg;
  if (static_cast<uint32_t>(reg) > static_cast<uint32_t>(REG::B)) {
    opcode_.rex = static_cast<uint8_t>(opcode_.rex) | static_cast<uint8_t>(REX::BASE);
  }
  return *this;
}

Instruction &Instruction::setM8_4RM(REG const baseReg, int32_t const displacement, REG const indexReg, uint32_t const indexScalePow2) VB_NOEXCEPT {
  rmType_ = RMType::MEM;
  rmBaseReg_ = baseReg;
  rmIndexReg_ = indexReg;
  rmIndexScalePow2_ = indexScalePow2;
  rmDisplacement_ = displacement;
  if (static_cast<uint32_t>(baseReg) > static_cast<uint32_t>(REG::B)) {
    opcode_.rex = static_cast<uint8_t>(opcode_.rex) | static_cast<uint8_t>(REX::BASE);
  }
  return *this;
}

// "Memory for RM"
// Set the rm field of an instruction to memory access (dereference). e.g.: ADD
// r32, rm32 -> ADD rax, [rbx + 4*rdx + 123456]. Scale multiplier for the index
// register is given as a power of two
Instruction &Instruction::setM4RM(REG const baseReg, int32_t const displacement, REG const indexReg, uint32_t const indexScalePow2) VB_NOEXCEPT {
  rmType_ = RMType::MEM;
  rmBaseReg_ = baseReg;
  rmIndexReg_ = indexReg;
  rmIndexScalePow2_ = indexScalePow2;
  rmDisplacement_ = displacement;
  return *this;
}
// "Memory Instruction Pointer for RM"
// Set the rm field of an instruction to memory access at a displacement from
// the current instruction pointer. e.g.: ADD r32, rm32 -> ADD rax, [rip + 100]
// Contrary to the official x86 instructions, we calculate the displacement
// from the start of that instruction
Instruction &Instruction::setMIP4RM(int32_t const displacementFromInstructionStart) VB_NOEXCEPT {
  rmType_ = RMType::MEM_RIP_DISPFROMINSTRSTART;
  rmDisplacement_ = displacementFromInstructionStart;
  return *this;
}
Instruction &Instruction::setMIP4RMabs(uint32_t const binaryPosition) VB_NOEXCEPT {
  int64_t const displacement{static_cast<int64_t>(binaryPosition) - static_cast<int64_t>(binary_.size())};
  assert(displacement >= INT32_MIN && displacement <= INT32_MAX && "MIP offset out of bounds");
  return setMIP4RM(static_cast<int32_t>(displacement));
}
// setImmX and setRelX set an immediate of the specified size for the
// instruction
Instruction &Instruction::setImm8(uint8_t const imm) VB_NOEXCEPT {
  immType_ = ImmType::IMM8;
  immediate_ = imm;
  return *this;
}
Instruction &Instruction::setImm16(uint16_t const imm) VB_NOEXCEPT {
  immType_ = ImmType::IMM16;
  immediate_ = imm;
  return *this;
}
Instruction &Instruction::setImm32(uint32_t const imm) VB_NOEXCEPT {
  immType_ = ImmType::IMM32;
  immediate_ = imm;
  return *this;
}
Instruction &Instruction::setImm64(uint64_t const imm) VB_NOEXCEPT {
  immType_ = ImmType::IMM64;
  immediate_ = imm;
  return *this;
}
Instruction &Instruction::setRel8(int8_t const rel) VB_NOEXCEPT {
  return setImm8(bit_cast<uint8_t>(rel));
}
Instruction &Instruction::setRel32(int32_t const rel) VB_NOEXCEPT {
  return setImm32(bit_cast<uint32_t>(rel));
}

// SetCC sets the condition code for an instruction, used after comparisons.
// Since condition codes are added to a base instruction,
Instruction &Instruction::setCC(CC const ccIn) VB_NOEXCEPT {
  this->cc_ = ccIn;
  return *this;
}

// Assembles an actual instruction, i.e. produces the machine code for the
// given inputs and writes it to the ed of the given MemWriter. Here is a good
// overview over x86 instruction encoding:
// http://www.c-jump.com/CIS77/CPU/x86/lecture.html (https://archive.is/m57SQ)
void Instruction::emitCode() {
  assert(!emitted_ && "Instruction can only be emitted once");

  // Set this to true before we try to write, otherwise the instruction might be destroyed without it being set to true
  // in an out of memory situation (exception from binary_.write<...>(...))
  emitted_ = true;

  uint32_t const startBinaryLength{binary_.size()};
  static_cast<void>(startBinaryLength);
  if (opcode_.prefix != 0x00U) {
    binary_.writeByte(opcode_.prefix);
  }

  if (cc_ != CC::NONE) {
    opcode_.opcode += static_cast<uint32_t>(cc_);
  }

  if (opcode_.extension == OPCodeExt::RADD) {
    // RADD will tell the assembler to add the rmBaseReg to the opcode
    opcode_.opcode += (static_cast<uint32_t>(rmBaseReg_) & 0b111U);
  } else if ((opcode_.extension != OPCodeExt::NONE) && (opcode_.extension != OPCodeExt::R)) {
    // Overwrite reg in REG/MOD/RM byte if opcode has an extension, the
    // underlying enum type will match the 3-bit integer
    // coverity[autosar_cpp14_a7_2_1_violation]
    rReg_ = static_cast<REG>(static_cast<uint8_t>(opcode_.extension));
  } else {
    static_cast<void>(0);
  }

  // Set the REX::R bit if rReg is a "new" (x86-64) register
  if ((static_cast<uint32_t>(rReg_) & 0b1000U) != 0U) {
    opcode_.rex = static_cast<uint8_t>(opcode_.rex) | static_cast<uint8_t>(REX::R);
  }
  // Set the REX::B bit if rmBaseReg is a "new" (x86-64) register
  if ((static_cast<uint32_t>(rmBaseReg_) & 0b1000U) != 0U) {
    opcode_.rex = static_cast<uint8_t>(opcode_.rex) | static_cast<uint8_t>(REX::B);
  }
  // Set REX::X bit if rmIndexReg  is a "new" (x86-64) register
  if ((static_cast<uint32_t>(rmIndexReg_) & 0b1000U) != 0U) {
    opcode_.rex = static_cast<uint8_t>(opcode_.rex) | static_cast<uint8_t>(REX::X);
  }

  // SIL DIL BPL SPL REX prefix
  if (opcode_.rex == REX::NONE) {
    if (((opcode_.extension == OPCodeExt::R) && ((static_cast<uint32_t>(rReg_) & 0b1111'1100U) == 0b100U)) &&
        ((static_cast<uint32_t>(opcode_.b8Flag) & 0b01U) != 0U)) {
      opcode_.rex = static_cast<uint8_t>(opcode_.rex) | static_cast<uint8_t>(REX::BASE);
    } else if (((rmType_ == RMType::REG) && ((static_cast<uint32_t>(rmBaseReg_) & 0b1111'1100U) == 0b100U)) &&
               ((static_cast<uint32_t>(opcode_.b8Flag) & 0b10U) != 0U)) {
      opcode_.rex = static_cast<uint8_t>(opcode_.rex) | static_cast<uint8_t>(REX::BASE);
    } else {
      static_cast<void>(0);
    }
  }

  if (opcode_.rex != REX::NONE) {
    binary_.writeByte(static_cast<uint8_t>(opcode_.rex));
  }

  // After an optional prefix byte and an optional REX byte, we now go ahead and
  // write the actual opcode. Zero bytes will be ignored. Will be emitted byte
  // by byte, starting with the most significant byte An opcode = 0x12003400
  // will produce "0x12 0x34" as machine code
  for (uint8_t i{0U}; i < 4U; i++) {
    uint32_t const offset{8U * (3U - static_cast<uint32_t>(i))};
    uint32_t const opcodeByte{(opcode_.opcode & (0xFF_U32 << offset)) >> offset};
    if (opcodeByte != 0x00U) {
      binary_.writeByte(static_cast<uint8_t>(opcodeByte));
    }
  }

  // Number of immediate bytes is encoding in the underlying enum type
  uint8_t const numImmediateBytes{static_cast<uint8_t>(immType_)};

  // Now go ahead and produce the MOD-REG-R/M byte and SIB (Scaled Index Byte)
  // if needed, but only if RMType is set
  if ((opcode_.extension != OPCodeExt::NONE) && (rmType_ != RMType::NONE)) {
    // MOD is encoded in the MOD-REG-RM byte as bits 6-7. (00 = [reg]; 01 = [reg
    // + disp8], 10 = [reg + disp32], 11 = reg) REG is bits 3-5, encoding the 3
    // least significant bits of the register index, the most significant bit is
    // encoded in the REX byte as REX::R, REG represents the non-RM register The
    // register encoded as the base in the RM field, 100 (= ESP/RSP) activates
    // SIB (Scaled Index Byte) mode, 101 (= EBP/RBP) activates [RIP + disp32]
    // mode
    constexpr uint32_t MODBitOffset{6U};
    constexpr uint32_t REGBitOffset{3U};
    constexpr uint32_t RMBitOffset{0U};
    // Encode the 3 LSBs in the REG part of the MOD-REG-RM byte, leave the other
    // parts undefined
    uint32_t modRegRMByte{(static_cast<uint32_t>(rReg_) & 0b111U) << static_cast<uint32_t>(REGBitOffset)};

    // If the RM part should be encoded as a direct (non-memory) register access
    if (rmType_ == RMType::REG) {
      // Set MOD to 11
      modRegRMByte |= 0b11_U32 << MODBitOffset;
      // Encode the RM register into the RM part of MOD-REG-RM, after that write
      // the MOD-REG-RM byte to memory
      uint32_t const rmBase{static_cast<uint32_t>(rmBaseReg_) & 0b111U};
      modRegRMByte |= rmBase << RMBitOffset;
      binary_.writeByte(static_cast<uint8_t>(modRegRMByte));
    } else if (rmType_ == RMType::MEM_RIP_DISPFROMINSTRSTART) {
      // Encode [RIP + disp32]. x86 encodes the disp32 offset from the end of
      // the executing instruction (i.e. a MOV eax, [rip + 0] reading the 4
      // bytes following the instruction doing the reading) MOD = 00 activates
      // displacement only addressing mode when RM is also 101
      modRegRMByte |= 0b00_U32 << MODBitOffset;
      // Set RM to 101, after that write to memory
      modRegRMByte |= 0b101_U32 << RMBitOffset;
      binary_.writeByte(static_cast<uint8_t>(modRegRMByte));

      // rmDisplacement is giving the displacement calculated from the start of
      // the instruction, x86 needs us to encode the displacement from the end
      // of the instruction, so we calculate the difference between those two
      // and adjust rmDisplacement accordingly
      uint32_t const numBytesEmitted{binary_.size() - startBinaryLength};
      // Number of bytes already produced (Prefix + REX + opcode + MOD-REG-RM) +
      // number of displacement bytes + immediate bytes that might follow
      uint32_t const displacementDelta{numBytesEmitted + 4U + numImmediateBytes};
      rmDisplacement_ -= static_cast<int32_t>(displacementDelta);
      // Displacement for [RIP + disp32] mode is always 4 (there is no option
      // for a byte displacement here)
      uint32_t const offset{bit_cast<uint32_t>(rmDisplacement_)};
      binary_.writeBytesLE(static_cast<uint64_t>(offset), 4U);
    } else {
      // Otherwise we are encoding a memory access with a base register, i.e.
      // [reg] with an optional (scaled) index register and an optional
      // displacement, i.e. we have RMType::MEM How many bytes we need to use to
      // encode the displacement (Memory dereference offset = [reg + disp]
      uint8_t displacementBytes{0U};
      // No displacement
      if (rmDisplacement_ == 0) {
        // If the base register is EBP/RBP (= 0b101) we need to have a
        // displacement, at least one byte because MOD = 00 and RM = 101
        // activates RIP displacement mode. This is even true for SIB mode
        if ((static_cast<uint32_t>(rmBaseReg_) & 0b111U) == 0b101U) {
          // One byte displacement
          modRegRMByte |= 0b01_U32 << MODBitOffset;
          // rmDisplacement is zero anyway, so we can simply copy the least
          // significant byte of that later
          displacementBytes = 1U;
        } else {
          // Zero byte displacement
          modRegRMByte |= 0b00_U32 << MODBitOffset;
          displacementBytes = 0U;
        }
      } else if ((rmDisplacement_ >= INT8_MIN) && (rmDisplacement_ <= INT8_MAX)) {
        // If displacement fits into a (signed!) INT8
        // One byte displacement
        modRegRMByte |= 0b01_U32 << MODBitOffset;
        displacementBytes = 1U;
      } else {
        // If displacement is not zero and does not fit into an INT8, we have to
        // use an INT32 displacement Four byte displacement
        modRegRMByte |= 0b10_U32 << MODBitOffset;
        displacementBytes = 4U;
      }

      // If we have no index register
      if (rmIndexReg_ == REG::NONE) {
        // Encode the base register into RM of the MOD-REG-RM byte
        uint32_t const rmBase{static_cast<uint32_t>(rmBaseReg_) & 0b111U};
        modRegRMByte |= rmBase << RMBitOffset;
        // MOD-REG-RM is finished, write to binary
        binary_.writeByte(static_cast<uint8_t>(modRegRMByte));
        // Since RM = 0b100 activates SIB mode and 0bx100 is the index of
        // ESP/RSP, we need to produce the SIB byte telling the CPU to use
        // [rsp]. The corresponding SIB byte is: 0b00100100
        if ((static_cast<uint32_t>(rmBaseReg_) & 0b111U) == 0b100U) {
          binary_.writeByte(0b00100100U);
        }
      } else {
        assert(rmIndexReg_ != REG::SP && "Stack pointer cannot be used for indexing");
        // Activate SIB (Scaled Index Byte) mode by setting RM to 100
        modRegRMByte |= 0b100_U32 << RMBitOffset;
        // MOD-REG-RM is finished
        binary_.writeByte(static_cast<uint8_t>(modRegRMByte));
        // Only Index*1, Index*2, Index*4, Index*8 allowed, power of two (0-3)
        // encoded in two bits
        assert(rmIndexScalePow2_ <= 0b11 && "Index scale for instruction out of range");

        // SIB is encoded as SCALE (2b)-INDEX (register as 3b)-BASE (register as
        // 3b)
        constexpr uint32_t SIBScaleBitOffset{6U};
        constexpr uint32_t SIBIndexBitOffset{3U};
        constexpr uint32_t SIBBaseBitOffset{0U};
        uint32_t const sibByteRaw{(rmIndexScalePow2_ << SIBScaleBitOffset) | ((static_cast<uint32_t>(rmIndexReg_) & 0b111_U32) << SIBIndexBitOffset) |
                                  ((static_cast<uint32_t>(rmBaseReg_) & 0b111_U32) << SIBBaseBitOffset)};
        uint8_t const sibByte{static_cast<uint8_t>(sibByteRaw)};
        binary_.writeByte(sibByte);
      }
      // Write the N least significant bytes of the displacement to the binary
      uint32_t const offset{bit_cast<uint32_t>(rmDisplacement_)};
      binary_.writeBytesLE(static_cast<uint64_t>(offset), displacementBytes);
    }
  }
  // Write the N least significant bytes of the immediate value at the end
  binary_.writeBytesLE(immediate_, numImmediateBytes);
}

} // namespace x86_64
} // namespace vb
#endif

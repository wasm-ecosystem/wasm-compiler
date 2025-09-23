///
/// @file aarch64_assembler.cpp
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
#include <array>
#include <cassert>
#include <cstdint>

#include "aarch64_assembler.hpp"
#include "aarch64_aux.hpp"
#include "aarch64_backend.hpp"
#include "aarch64_cc.hpp"
#include "aarch64_encoding.hpp"
#include "aarch64_relpatchobj.hpp"

#include "src/core/common/FunctionRef.hpp"
#include "src/core/common/Span.hpp"
#include "src/core/common/TrapCode.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/implementationlimits.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/Compiler.hpp"
#include "src/core/compiler/backend/aarch64/aarch64_instruction.hpp"
#include "src/core/compiler/common/Common.hpp"
#include "src/core/compiler/common/MachineType.hpp"
#include "src/core/compiler/common/MemWriter.hpp"
#include "src/core/compiler/common/ModuleInfo.hpp"
#include "src/core/compiler/common/RegMask.hpp"
#include "src/core/compiler/common/SafeInt.hpp"
#include "src/core/compiler/common/StackElement.hpp"
#include "src/core/compiler/common/VariableStorage.hpp"
#include "src/core/compiler/common/util.hpp"

namespace vb {
namespace aarch64 {
using Assembler = AArch64_Assembler; ///< Shortcut for AArch64_Assembler

Assembler::AArch64_Assembler(AArch64_Backend &backend, MemWriter &binary, ModuleInfo &moduleInfo) VB_NOEXCEPT : backend_(backend),
                                                                                                                binary_(binary),
                                                                                                                moduleInfo_(moduleInfo),
                                                                                                                lastTrapPosition_() {
}

Instruction Assembler::INSTR(OPCodeTemplate const opcode) const VB_NOEXCEPT {
#if ENABLE_EXTENSIONS
  if (backend_.compiler_.getDwarfGenerator() != nullptr) {
    backend_.compiler_.getDwarfGenerator()->record(binary_.size());
  }
#endif
  return Instruction(opcode, binary_);
}

Instruction Assembler::INSTR(AbstrInstr const abstrInstr) const VB_NOEXCEPT {
#if ENABLE_EXTENSIONS
  if (backend_.compiler_.getDwarfGenerator() != nullptr) {
    backend_.compiler_.getDwarfGenerator()->record(binary_.size());
  }
#endif
  return Instruction(abstrInstr, binary_);
}

void Assembler::TRAP(TrapCode const trapCode) const {
  assert(moduleInfo_.helperFunctionBinaryPositions.genericTrapHandler != 0xFF'FF'FF'FFU && "miss generic trap handler");
  if (backend_.compiler_.getDebugMode()) {
    // quick path for debug mode
    MOVimm32(WasmABI::REGS::trapPosReg, moduleInfo_.bytecodePosOfLastParsedInstruction);
    if (trapCode != TrapCode::NONE) {
      MOVimm32(WasmABI::REGS::trapReg, static_cast<uint32_t>(trapCode));
    }
    prepareJMP().linkToBinaryPos(moduleInfo_.helperFunctionBinaryPositions.genericTrapHandler);
    return;
  }
  uint32_t lastPos{0U};
  if (lastTrapPosition_.get(trapCode, binary_.size(), lastPos)) {
    prepareJMP(CC::NONE).linkToBinaryPos(lastPos);
    return;
  }
  if (trapCode != TrapCode::NONE) {
    // mov trapReg trapCode
    lastTrapPosition_.set(trapCode, binary_.size());
    MOVimm32(WasmABI::REGS::trapReg, static_cast<uint32_t>(trapCode));
  }
  if (in_range<28U>(static_cast<int64_t>(binary_.size()) - static_cast<int64_t>(moduleInfo_.helperFunctionBinaryPositions.genericTrapHandler))) {
    lastTrapPosition_.set(TrapCode::NONE, binary_.size());
    prepareJMP().linkToBinaryPos(moduleInfo_.helperFunctionBinaryPositions.genericTrapHandler);
  } else if (lastTrapPosition_.get(TrapCode::NONE, binary_.size(), lastPos)) {
    prepareJMP().linkToBinaryPos(lastPos);
  } else {
    throw ImplementationLimitationException(ErrorCode::Branches_can_only_target_offsets_in_the_range___128MB);
  }
}

void Assembler::cTRAP(TrapCode const trapCode, CC const conditionCode) const {
  uint32_t lastTrapJitCodePosition{0U};
  if (lastTrapPosition_.get(trapCode, binary_.size(), lastTrapJitCodePosition)) {
    // We never enable this optimization in debug mode
    assert(!backend_.compiler_.getDebugMode());
    // If the last trap JIT code can be reached, we can jump to last trap JIT code by conditional jump to reduce
    // instruction counts.
    prepareJMP(conditionCode).linkToBinaryPos(lastTrapJitCodePosition);
    return;
  }
  RelPatchObj const relPatchObj{prepareJMP(negateCC(conditionCode))};
  TRAP(trapCode);
  relPatchObj.linkToHere();
}

void Assembler::addImm24ToReg(REG const dstReg, int32_t const delta, bool const is64, REG srcReg) const {
  assert(RegUtil::isGPR(dstReg) && "Register not a GPR");

  // coverity[autosar_cpp14_m5_0_9_violation]
  uint32_t const absDelta{static_cast<uint32_t>((delta < 0) ? -delta : delta)};
  assert(absDelta <= 0xFF'FF'FF && "Immediate too large");

  if (srcReg == REG::NONE) {
    srcReg = dstReg;
  }

  if (delta < 0) {
    AbstrInstr const instr{is64 ? SUB_xD_xN_imm12zxols12 : SUB_wD_wN_imm12zxols12};
    if ((absDelta & 0xFFFU) != 0U) {
      INSTR(instr).setD(dstReg).setN(srcReg).setImm12zx(SafeUInt<12U>::max() & absDelta)();
      srcReg = dstReg;
    }
    if ((absDelta & 0xFF'F0'00U) != 0U) {
      INSTR(instr).setD(dstReg).setN(srcReg).setImm12zxls12(SafeUInt<24U>::max() & absDelta)();
    }
  } else if (delta > 0) {
    AbstrInstr const instr{is64 ? ADD_xD_xN_imm12zxols12 : ADD_wD_wN_imm12zxols12};
    if ((absDelta & 0xFFFU) != 0U) {
      INSTR(instr).setD(dstReg).setN(srcReg).setImm12zx(SafeUInt<12U>::max() & absDelta)();
      srcReg = dstReg;
    }
    if ((absDelta & 0xFF'F0'00U) != 0U) {
      INSTR(instr).setD(dstReg).setN(srcReg).setImm12zxls12(SafeUInt<24U>::max() & absDelta)();
    }
  } else {
    static_cast<void>(0);
  }
  // If delta is zero don't do anything
}

void Assembler::addImmToReg(REG const reg, int64_t const delta, bool const is64, RegMask const protRegs, REG intermReg) const {
  assert(RegUtil::isGPR(reg) && "Register not a GPR");

  // coverity[autosar_cpp14_m5_0_9_violation]
  uint64_t const absDelta{static_cast<uint64_t>((delta < 0) ? -delta : delta)};
  if (absDelta <= 0xFF'FF'FFU) {
    addImm24ToReg(reg, static_cast<int32_t>(delta), is64);
  } else {
    if (intermReg == REG::NONE) {
      RegAllocTracker tempRegAllocTracker{};
      tempRegAllocTracker.writeProtRegs = protRegs;
      intermReg = backend_.common_.reqScratchRegProt(MachineType::I64, tempRegAllocTracker, false).reg;
    }

    MOVimm64(intermReg, absDelta);

    if (delta < 0) {
      INSTR(is64 ? SUB_xD_xN_xMolsImm6 : SUB_wD_wN_wMolsImm6).setD(reg).setN(reg).setM(intermReg)();
    } else {
      INSTR(is64 ? ADD_xD_xN_xMolsImm6 : ADD_wD_wN_wMolsImm6).setD(reg).setN(reg).setM(intermReg)();
    }
  }
}

void Assembler::addImmToReg(REG const dstReg, REG const srcReg, int64_t const delta, bool const is64) const {
  assert(srcReg != dstReg);
  assert(RegUtil::isGPR(srcReg) && "Register not a GPR");
  assert(RegUtil::isGPR(dstReg) && "Register not a GPR");

  // coverity[autosar_cpp14_m5_0_9_violation]
  uint64_t const absDelta{static_cast<uint64_t>((delta < 0) ? -delta : delta)};
  if (absDelta <= 0xFF'FF'FFU) {
    addImm24ToReg(dstReg, static_cast<int32_t>(delta), is64, srcReg);
  } else {
    MOVimm64(dstReg, absDelta);
    if (delta < 0) {
      INSTR(is64 ? SUB_xD_xN_xMolsImm6 : SUB_wD_wN_wMolsImm6).setD(dstReg).setN(srcReg).setM(dstReg)();
    } else {
      INSTR(is64 ? ADD_xD_xN_xMolsImm6 : ADD_wD_wN_wMolsImm6).setD(dstReg).setN(srcReg).setM(dstReg)();
    }
  }
}

void Assembler::setStackFrameSize(uint32_t const frameSize, bool const temporary, bool const mayRemoveLocals) {
  assert((frameSize == moduleInfo_.getStackFrameSizeBeforeReturn()) || frameSize == alignStackFrameSize(frameSize));
  assert(frameSize >= moduleInfo_.getStackFrameSizeBeforeReturn() && "Cannot remove return address and parameters");

  if (!mayRemoveLocals) {
    assert(frameSize >= moduleInfo_.getFixedStackFrameWidth() && "Cannot implicitly drop active variables (tempstack, local) by truncating stack");
  }

  if (moduleInfo_.fnc.stackFrameSize != frameSize) {
    // This is the maximum allowed stack frame since we are using addImm24ToReg to modify the stack pointer here
    constexpr uint32_t maxAllowedStackFrameSize{(1_U32 << 24_U32) - 1_U32};
    static_assert(maxAllowedStackFrameSize >= ImplementationLimits::maxStackFrameSize, "Maximum stack frame size too large");
    if (frameSize > ImplementationLimits::maxStackFrameSize) {
      throw ImplementationLimitationException(ErrorCode::Reached_maximum_stack_frame_size);
    }

    int64_t const delta{static_cast<int64_t>(moduleInfo_.fnc.stackFrameSize) - static_cast<int64_t>(frameSize)};
    addImm24ToReg(REG::SP, static_cast<int32_t>(delta), true);

    if (!temporary) {
      moduleInfo_.fnc.stackFrameSize = frameSize;
    }
  }

#if ENABLE_EXTENSIONS
  if (backend_.compiler_.getAnalytics() != nullptr) {
    backend_.compiler_.getAnalytics()->updateMaxStackFrameSize(frameSize);
  }
#endif
}

uint32_t Assembler::alignStackFrameSize(uint32_t const frameSize) const VB_NOEXCEPT {
  // Align to 16B (without params)
  return roundUpToPow2(frameSize - moduleInfo_.fnc.paramWidth, 4U) + moduleInfo_.fnc.paramWidth;
}

#if ACTIVE_STACK_OVERFLOW_CHECK
void Assembler::checkStackFence(REG const scratchReg) {
  assert(scratchReg != REG::NONE && "Scratch register needed");
  INSTR(LDUR_xT_deref_xN_unscSImm9_t)
      .setT(scratchReg)
      .setN(WasmABI::REGS::linMem)
      .setUnscSImm9(SafeInt<9>::fromConst<-Basedata::FromEnd::stackFence>())();
  INSTR(CMP_SP_xM_t).setM(scratchReg)();
  RelPatchObj const inRange = prepareJMP(CC::HS);
  TRAP(TrapCode::STACKFENCEBREACHED);
  inRange.linkToHere();
}
#endif

void Assembler::probeStack(uint32_t const delta, REG const scratchReg1, REG const scratchReg2) const {
  assert(scratchReg1 != REG::NONE && scratchReg2 != REG::NONE && "Scratch register needed");

  // Smallest possible page size on AArch64
  constexpr uint32_t osPageSize{1_U32 << 12_U32};
  if (delta < osPageSize) {
    return;
  }

  // Move SP to scratchReg1
  INSTR(ADD_xD_xN_imm12zxols12).setD(scratchReg1).setN(REG::SP).setImm12zx(SafeUInt<12U>::fromConst<0U>())();
  MOVimm64(scratchReg2, static_cast<uint64_t>(delta));

#ifdef VB_WIN32
  uint32_t const branchTargetOffset = binary_.size();
  INSTR(SUB_xD_xN_imm12zxols12).setD(scratchReg1).setN(scratchReg1).setImm12zxls12(SafeUInt<24U>::fromConst<osPageSize>())();

  // Probe the position and discard the result
  INSTR(LDR_xT_deref_xN_imm12zxls3_t).setT(REG::ZR).setN(scratchReg1).setImm12zxls3(SafeUInt<15U>::fromConst<0U>())();

  INSTR(SUBS_xD_xN_imm12zxols12).setD(scratchReg2).setN(scratchReg2).setImm12zxls12(SafeUInt<24U>::fromConst<osPageSize>())();
  prepareJMP(CC::GT).linkToBinaryPos(branchTargetOffset);
#else
  uint32_t const branchTargetOffset{binary_.size()};
  INSTR(SUB_xD_xN_imm12zxols12).setD(REG::SP).setN(REG::SP).setImm12zxls12(SafeUInt<24U>::fromConst<osPageSize>())();

  // Probe the position and discard the result
  INSTR(LDR_xT_deref_xN_imm12zxls3_t).setT(REG::ZR).setN(REG::SP).setImm12zxls3(SafeUInt<15U>::fromConst<0>())();

  INSTR(SUBS_xD_xN_imm12zxols12).setD(scratchReg2).setN(scratchReg2).setImm12zxls12(SafeUInt<24U>::fromConst<osPageSize>())();
  prepareJMP(CC::GT).linkToBinaryPos(branchTargetOffset);

  // Restore the stack pointer
  INSTR(ADD_xD_xN_imm12zxols12).setD(REG::SP).setN(scratchReg1).setImm12zx(SafeUInt<12U>::fromConst<0U>())();
#endif
}

bool Assembler::elementFitsArgType(ArgType const argType, VariableStorage const &storage) const VB_NOEXCEPT {
  if (argType == ArgType::NONE) {
    return true;
  }
  if (storage.type == StorageType::INVALID) {
    return false;
  }

  if (storage.type == StorageType::INVALID) {
    return false;
  } else if (storage.type == StorageType::CONSTANT) {
    if (storage.machineType == MachineType::I32) {
      if ((argType == ArgType::imm6l_32) || (argType == ArgType::imm6r_32)) {
        return true;
      }
      if (argType == ArgType::imm12zxols12_32) {
        if (storage.location.constUnion.u32 <= 0xFFFU) {
          return true;
        }
        if (((storage.location.constUnion.u32 & 0xFFFU) == 0U) && ((storage.location.constUnion.u32 >> 12U) <= 0xFFFU)) {
          return true;
        }
      }
      if (argType == ArgType::imm12bitmask_32) {
        uint64_t encoding;
        return processLogicalImmediate(static_cast<uint64_t>(storage.location.constUnion.u32), false, encoding);
      }
    } else if (storage.machineType == MachineType::I64) {
      if ((argType == ArgType::imm6l_64) || (argType == ArgType::imm6r_64)) {
        return true;
      }
      if (argType == ArgType::imm12zxols12_64) {
        if (storage.location.constUnion.u64 <= 0xFFFU) {
          return true;
        }
        if (((storage.location.constUnion.u64 & 0xFFFU) == 0U) && ((storage.location.constUnion.u64 >> 12U) <= 0xFFFU)) {
          return true;
        }
      }
      if (argType == ArgType::imm13bitmask_64) {
        uint64_t encoding;
        return processLogicalImmediate(storage.location.constUnion.u64, true, encoding);
      }
    } else {
      static_cast<void>(0);
    }
  } else if (storage.type == StorageType::REGISTER) {
    if (MachineTypeUtil::isInt(storage.machineType) && ((argType == ArgType::r32) || (argType == ArgType::r64))) {
      return true;
    }
    if ((storage.machineType == MachineType::F32) && (argType == ArgType::r32f)) {
      return true;
    }
    if ((storage.machineType == MachineType::F64) && (argType == ArgType::r64f)) {
      return true;
    }
  } else {
    static_cast<void>(0);
  }
  return false;
}

void Assembler::emitActionArg(AbstrInstr const actionArg, VariableStorage const &dest, VariableStorage const &src0, VariableStorage const &src1) {
  assert(elementFitsArgType(actionArg.dstType, dest) && "Argument doesn't fit instruction");
  assert(elementFitsArgType(actionArg.src0Type, src0) && "Argument doesn't fit instruction");
  assert(elementFitsArgType(actionArg.src1Type, src1) && "Argument doesn't fit instruction");
  Instruction instruction{INSTR(actionArg.opcode)};
  uint8_t numImmediates{0U};
  static_cast<void>(numImmediates);
  using SetFunc = Instruction &(Instruction::*)(REG const);
  // coverity[autosar_cpp14_a8_5_2_violation]
  auto const setInstructionOperand =
      // coverity[autosar_cpp14_m7_1_2_violation]
      [&instruction, &numImmediates](VariableStorage const &storage, ArgType const argType, SetFunc const setFunc) -> void {
    if (storage.type == StorageType::INVALID) {
      return;
    }
    if (argType == ArgType::NONE) {
      return;
    }

    if (((argType == ArgType::r32) || (argType == ArgType::r64)) || ((argType == ArgType::r32f) || (argType == ArgType::r64f))) {
      // coverity[autosar_cpp14_a8_5_2_violation]
      static_cast<void>((instruction.*setFunc)(storage.location.reg));
    } else {
      assert(numImmediates == 0 && "Multiple immediates not possible in one instruction");
      numImmediates++;
      if ((argType == ArgType::imm12zxols12_32) || (argType == ArgType::imm12zxols12_64)) {
        uint32_t immValue;
        if (argType == ArgType::imm12zxols12_32) {
          immValue = storage.location.constUnion.u32;
        } else {
          assert(storage.location.constUnion.u64 <= UINT32_MAX && "Value too big");
          immValue = static_cast<uint32_t>(storage.location.constUnion.u64);
        }
        UnsignedInRangeCheck<12> const rangeCheck12{UnsignedInRangeCheck<12>::check(immValue)};
        if (rangeCheck12.inRange()) {
          static_cast<void>(instruction.setImm12zx(rangeCheck12.safeInt()));
        } else {
          UnsignedInRangeCheck<24> const rangeCheck24{UnsignedInRangeCheck<24>::check(immValue)};
          if (rangeCheck24.inRange()) {
            static_cast<void>(instruction.setImm12zxls12(rangeCheck24.safeInt()));
          }
          // GCOVR_EXCL_START
          else {
            UNREACHABLE(_, "before enter this function, the arg value are already checked if it's imm12 or imm12ls2. So "
                           "no other cases");
          }
          // GCOVR_EXCL_STOP
        }
      } else if (argType == ArgType::imm12bitmask_32) {
        static_cast<void>(instruction.setImmBitmask(static_cast<uint64_t>(storage.location.constUnion.u32)));
      } else if (argType == ArgType::imm13bitmask_64) {
        static_cast<void>(instruction.setImmBitmask(storage.location.constUnion.u64));
      } else if (((argType == ArgType::imm6l_32) || (argType == ArgType::imm6r_32)) ||
                 ((argType == ArgType::imm6l_64) || (argType == ArgType::imm6r_64))) {
        bool const isLeft{(argType == ArgType::imm6l_32) || (argType == ArgType::imm6l_64)};
        bool const is64{(argType == ArgType::imm6l_64) || (argType == ArgType::imm6r_64)};
        if (is64) {
          static_cast<void>(
              instruction.setImm6x(isLeft, SafeUInt<6U>::fromConst<0b0011'1111U>() & static_cast<uint32_t>(storage.location.constUnion.u64)));
        } else {
          static_cast<void>(instruction.setImm6x(isLeft, SafeUInt<6U>::fromConst<0b0001'1111U>() & storage.location.constUnion.u32));
        }

      } else {
        static_cast<void>(0);
      }
    }
  };
  // coverity[autosar_cpp14_a4_5_1_violation]
  setInstructionOperand(dest, actionArg.dstType, &Instruction::setD);
  // coverity[autosar_cpp14_a4_5_1_violation]
  setInstructionOperand(src0, actionArg.src0Type, &Instruction::setN);
  // coverity[autosar_cpp14_a4_5_1_violation]
  setInstructionOperand(src1, actionArg.src1Type, &Instruction::setM);
  instruction();
}

bool Assembler::FMOVimm(bool const is64, REG const reg, uint64_t const rawFloatImm) const {
  assert((reg == REG::NONE || !RegUtil::isGPR(reg)) && "Only FPR registers allowed");
  assert((is64 || rawFloatImm <= UINT32_MAX) && "Imm too large");

  if (rawFloatImm == 0U) {
    if (reg != REG::NONE) {
      INSTR(is64 ? FMOV_dD_xN : FMOV_sD_wN).setD(reg).setN(REG::ZR)();
    }
    return true;
  } else {
    uint64_t const N{is64 ? 64_U64 : 32_U64};
    uint64_t const E{is64 ? 11_U64 : 8_U64};
    uint64_t const F{(N - E) - 1_U64}; // From bits(N) VFPExpandImm(bits(8) imm8)
    uint64_t const rawExponent{(rawFloatImm >> F) & ((1_U64 << E) - 1U)};
    uint64_t const rawMantissa{rawFloatImm & ((1_U64 << F) - 1U)};
    uint64_t const rawEncodedExponent{rawExponent >> 2U};
    bool const exponentCanBeEncoded{(rawEncodedExponent == (1_U64 << (E - 3U))) ||
                                    (rawEncodedExponent == ((1_U64 << (E - 3U)) - 1U))}; // Check whether the most significant bit
                                                                                         // of the exponent is NOT the next (E - 3)
                                                                                         // bits and those (E - 3) bits are uniform
    if (!exponentCanBeEncoded) {
      return false;
    }
    bool const mantissaCanBeEncoded{rawMantissa == ((rawMantissa >> (F - 4U)) << (F - 4U))}; // Check whether only the most significant
                                                                                             // 4 bits of the mantissa are non-zero
    if (!mantissaCanBeEncoded) {
      return false;
    }

    uint64_t rawEncoding{0U};
    rawEncoding |= ((rawFloatImm >> (F + E)) & 0b1U) << 7U; // Sign
    rawEncoding |= ((rawFloatImm >> F) & 0b111U) << 4U;     // Exponent
    rawEncoding |= (rawFloatImm >> (F - 4U)) & 0xFU;        // Mantissa

    if (reg != REG::NONE) {
      INSTR(is64 ? FMOV_dD_imm8mod_t : FMOV_sD_imm8mod_t).setD(reg).setRawFMOVImm8(static_cast<uint32_t>(rawEncoding))();
    }

    return true;
  }
}

void Assembler::MOVimm(bool const is64, REG const reg, uint64_t const imm) const {
  assert(RegUtil::isGPR(reg) && "Only GPR registers allowed");

  uint8_t numZeroHalfwords{0U};
  uint8_t numFFFFHalfwords{0U};
  uint8_t const numHalfWordsInReg{is64 ? static_cast<uint8_t>(4U) : static_cast<uint8_t>(2U)};
  for (uint8_t i{0U}; i < numHalfWordsInReg; i++) {
    uint64_t const halfword{(imm >> (static_cast<uint64_t>(i) * 16U)) & static_cast<uint64_t>(UINT16_MAX)};
    if (halfword == 0x0000U) {
      numZeroHalfwords++;
    } else if (halfword == 0xFFFFU) {
      numFFFFHalfwords++;
    } else {
      static_cast<void>(0);
    }
  }

  if ((numZeroHalfwords < (numHalfWordsInReg - 1U)) && (numFFFFHalfwords < (numHalfWordsInReg - 1U))) { // Try bitmask encoding
    uint64_t encoding;
    if (processLogicalImmediate(imm, is64,
                                encoding)) { // Bitmask encoding is possible
      return INSTR(is64 ? MOV_xD_imm13bitmask_t : MOV_wD_imm12bitmask_t).setD(reg).setRawImmBitmask(static_cast<uint32_t>(encoding))();
    }
  }

  bool firstHalfwordIsSet{false};
  for (uint32_t i{0U}; i < numHalfWordsInReg; i++) {
    uint64_t const halfWordRaw{imm >> (static_cast<uint64_t>(i) * 16ULL)};
    SafeUInt<16> const halfword{SafeUInt<16>::max() & static_cast<uint32_t>(halfWordRaw)};
    if (numZeroHalfwords >= numFFFFHalfwords) { // Use movz
      if (halfword.value() != 0x0000U) {
        if (!firstHalfwordIsSet) {
          INSTR(is64 ? MOVZ_xD_imm16ols_t : MOVZ_wD_imm16ols_t).setD(reg).setImm16Ols(halfword, i * 16_U32)();
          firstHalfwordIsSet = true;
        } else {
          INSTR(is64 ? MOVK_xD_imm16ols_t : MOVK_wD_imm16ols_t).setD(reg).setImm16Ols(halfword, i * 16_U32)();
        }
      } else if (numZeroHalfwords == numHalfWordsInReg) {
        assert(i == 0 && "Error");
        INSTR(is64 ? MOVZ_xD_imm16ols_t : MOVZ_wD_imm16ols_t).setD(reg).setImm16Ols(halfword, i * 16_U32)();
        break;
      } else {
        static_cast<void>(0);
      }
    } else { // Use movn
      SafeUInt<16U> const notHalfWord{SafeUInt<16U>::max() & ~halfword.value()};

      if (halfword.value() != 0xFFFFU) {
        if (!firstHalfwordIsSet) {
          INSTR(is64 ? MOVN_xD_imm16ols_t : MOVN_wD_imm16ols_t).setD(reg).setImm16Ols(notHalfWord, i * 16_U32)();
          firstHalfwordIsSet = true;
        } else {
          INSTR(is64 ? MOVK_xD_imm16ols_t : MOVK_wD_imm16ols_t).setD(reg).setImm16Ols(halfword, i * 16_U32)();
        }
      } else if (numFFFFHalfwords == numHalfWordsInReg) {
        assert(i == 0 && "Error");
        INSTR(is64 ? MOVN_xD_imm16ols_t : MOVN_wD_imm16ols_t).setD(reg).setImm16Ols(notHalfWord, i * 16_U32)();
        break;
      } else {
        static_cast<void>(0);
      }
    }
  }
}

Assembler::ActionResult Assembler::selectInstr(Span<AbstrInstr const> const &instructions, std::array<VariableStorage, 2U> &inputStorages,
                                               std::array<bool const, 2> const startedAsWritableScratchReg, StackElement const *const targetHint,
                                               RegMask const protRegs, bool const presFlags) VB_THROW {
  assert(instructions.size() > 0 && "Zero instructions to select from");

  MachineType const dstType{getMachineTypeFromArgType(instructions[0].dstType)};
  // coverity[autosar_cpp14_a8_5_2_violation]
  const auto srcTypes = make_array(getMachineTypeFromArgType(instructions[0].src0Type), getMachineTypeFromArgType(instructions[0].src1Type));

  bool const src_0_1_commutative{instructions[0].src_0_1_commutative};
  bool const unop{instructions[0].src1Type == ArgType::NONE};
  bool const actionIsReadonly{instructions[0].dstType == ArgType::NONE};

  assert((!unop || !src_0_1_commutative) && "Unary operation cannot be commutative");
#ifndef NDEBUG
  if (unop) {
    assert(inputStorages[0].type != StorageType::INVALID && "Unary operation mandates only 1 argument");
    assert(inputStorages[1].type == StorageType::INVALID && "Unary operation mandates only 1 argument");
  } else {
    for (uint32_t i{0U}; i < 2U; i++) {
      if (srcTypes[i] == MachineType::INVALID) {
        assert(inputStorages[i].type == StorageType::INVALID && "Invalid source MachineType mandates INVALID as input");
      } else {
        assert(inputStorages[i].type != StorageType::INVALID && "Source argument missing, even though instruction mandates one");
      }
    }
  }
#endif

  REG const targetHintReg{backend_.getUnderlyingRegIfSuitable(targetHint, dstType, protRegs)};
  StackElement const *const verifiedTargetHint{(targetHintReg != REG::NONE) ? targetHint : nullptr};

  std::array<bool, 2> argCanBeDst{};
  if (!actionIsReadonly) {
    argCanBeDst[0] =
        startedAsWritableScratchReg[0] || ((verifiedTargetHint != nullptr) && inputStorages[0].equals(moduleInfo_.getStorage(*verifiedTargetHint)));
    argCanBeDst[1] =
        startedAsWritableScratchReg[1] || ((verifiedTargetHint != nullptr) && inputStorages[1].equals(moduleInfo_.getStorage(*verifiedTargetHint)));
  } else {
    argCanBeDst[0] = false;
    argCanBeDst[1] = false;
  }

  // Check whether both are equal to another and not INVALID
  bool const argsAreEqual{inputStorages[0].equals(inputStorages[1])};

  // Lambda functions that can be used to lift the arguments
  std::array<bool, 2> argHasBeenLifted{{false, false}};
  bool changed{false};
  // coverity[autosar_cpp14_a8_5_2_violation]
  auto liftArgLambda = [this, &inputStorages, verifiedTargetHint, &argCanBeDst, argsAreEqual, &changed, protRegs, &argHasBeenLifted,
                        presFlags](uint32_t const idx, bool const coLift = false) mutable {
    assert(!argHasBeenLifted[idx] && "Cannot lift arg twice");
    assert((!protRegs.allMarked()) && "Cannot lift");
    assert(idx <= 1U && "Lift index out of range"); // As we only have two args, idx must be 0 or 1

    // otherIdx is 1 if idx is 0, else otherIdx is 0
    uint32_t const otherIdx{idx ^ 1U};
    if (argsAreEqual && argHasBeenLifted[otherIdx]) {
      inputStorages[idx] = inputStorages[otherIdx];
    } else {
      RegAllocTracker tempRegAllocTracker{};
      tempRegAllocTracker.writeProtRegs = protRegs | backend_.mask(inputStorages[otherIdx]);
      REG const scratchReg{
          backend_.common_.reqScratchRegProt(inputStorages[idx].machineType, verifiedTargetHint, tempRegAllocTracker, presFlags).reg};
      VariableStorage const newStorage{VariableStorage::reg(scratchReg, inputStorages[idx].machineType)};
      backend_.emitMoveImpl(newStorage, inputStorages[idx], false);
      inputStorages[idx] = newStorage;
    }

    // Lifted arg can now be dest, as it's now guaranteed to be in a writable
    // register
    argCanBeDst[idx] = true;
    argHasBeenLifted[idx] = true;
    changed = true;

    // If both args are equal, set the other arg to the newly lifted one and
    // also set argCanBeDst accordingly
    if ((coLift && argsAreEqual) && (!argHasBeenLifted[otherIdx])) {
      inputStorages[otherIdx] = inputStorages[idx];
      argCanBeDst[otherIdx] = true;
      argHasBeenLifted[otherIdx] = true;
    }
  };

  // Prelift
  assert(inputStorages[0].type != StorageType::INVALID);
  if ((inputStorages[0].type == StorageType::STACKMEMORY) || (inputStorages[0].type == StorageType::LINKDATA)) {
    liftArgLambda(0U, true);
  }
  if ((!unop) && (!argHasBeenLifted[1])) {
    if ((inputStorages[1].type == StorageType::STACKMEMORY) || (inputStorages[1].type == StorageType::LINKDATA)) {
      liftArgLambda(1U);
    }
  }

  std::array<bool, 2> argHasMatched{{false, false}};
  for (uint32_t tries{0U}; tries < 2U; tries++) {
    for (uint32_t instrIdx{0U}; instrIdx < instructions.size(); instrIdx++) {
      // Current instruction from the array of given instructions which will be
      // checked for matching arguments
      AbstrInstr const actionArg{instructions[instrIdx]};
      assert(actionArg.src_0_1_commutative == src_0_1_commutative && (actionArg.src1Type == ArgType::NONE) == unop &&
             "Choosable instructions must be uniformly commutative or unop");
      assert(dstType == getMachineTypeFromArgType(actionArg.dstType) && srcTypes[0] == getMachineTypeFromArgType(actionArg.src0Type) &&
             srcTypes[1] == getMachineTypeFromArgType(actionArg.src1Type) && "Choosable instructions must have uniformly typed ArgTypes");

      // If it's no unary operation, it is commutative and the args are not
      // equal, we have two commutation tries (one for each order), otherwise we
      // only check the given order
      bool const checkReversedOrder{src_0_1_commutative && (!argsAreEqual)};
      uint32_t const commutationTries{checkReversedOrder ? 2_U32 : 1_U32};

      for (uint32_t firstArgIdx{0U}; firstArgIdx < commutationTries; firstArgIdx++) {
        uint32_t const secondArgIdx{firstArgIdx ^ 1U};

        std::array<bool, 2> argMatches{{false, false}};
        argMatches[firstArgIdx] = elementFitsArgType(actionArg.src0Type, inputStorages[firstArgIdx]);
        argMatches[secondArgIdx] = elementFitsArgType(actionArg.src1Type, inputStorages[secondArgIdx]);

        argHasMatched[firstArgIdx] = argMatches[firstArgIdx] || argHasMatched[firstArgIdx];
        argHasMatched[secondArgIdx] = argMatches[secondArgIdx] || argHasMatched[secondArgIdx];

        if (argsAreEqual) {
          argHasMatched[firstArgIdx] = argMatches[secondArgIdx] || argHasMatched[firstArgIdx];
          argHasMatched[secondArgIdx] = argMatches[firstArgIdx] || argHasMatched[secondArgIdx];
        }

        if (argMatches[0] && argMatches[1]) {
          Assembler::ActionResult actionResult{};
          if (dstType == MachineType::INVALID) {
            // do nothing
          } else if (verifiedTargetHint != nullptr) {
            VariableStorage const verifiedTargetHintStorage{moduleInfo_.getStorage(*verifiedTargetHint)};
            assert((verifiedTargetHintStorage.type == StorageType::REGISTER) && "Invalid target hint");
            // Need to rebuild a storage with dstType, because targetHint's storage type may not match dstType in mix using i32 and i64 reg case
            actionResult.storage = VariableStorage::reg(verifiedTargetHintStorage.location.reg, dstType);
          } else if (argCanBeDst[firstArgIdx] &&
                     ((srcTypes[firstArgIdx] == dstType) || (MachineTypeUtil::isInt(srcTypes[firstArgIdx]) && MachineTypeUtil::isInt(dstType)))) {
            actionResult.storage = inputStorages[firstArgIdx];
            actionResult.storage.machineType = dstType;
          } else if (argCanBeDst[secondArgIdx] &&
                     ((srcTypes[secondArgIdx] == dstType) || (MachineTypeUtil::isInt(srcTypes[secondArgIdx]) && MachineTypeUtil::isInt(dstType)))) {
            actionResult.storage = inputStorages[secondArgIdx];
            actionResult.storage.machineType = dstType;
          } else {
            RegAllocTracker fullRegAllocTracker{};
            fullRegAllocTracker.readProtRegs = protRegs | backend_.mask(inputStorages[0]) | backend_.mask(inputStorages[1]);
            RegElement const regElement{backend_.common_.reqScratchRegProt(dstType, verifiedTargetHint, fullRegAllocTracker, presFlags)};
            actionResult.storage = VariableStorage::reg(regElement.reg, dstType);
          }
          emitActionArg(actionArg, actionResult.storage, inputStorages[firstArgIdx], inputStorages[secondArgIdx]);

          actionResult.reversed = firstArgIdx != 0U;
          return actionResult;
        }
      }
    }

    // Either lifting into registers is protected by protRegs and the first
    // instruction didn't match or we have already lifted both and there is
    // still no instruction that matches the arguments This should never happen
    // if validation is done before calling this function
    assert((!protRegs.allMarked()) && tries < 2 && "Instruction selection error");

    if (unop) { // lift and try again
      if (!argHasMatched[0]) {
        liftArgLambda(0U);
      }
    } else {
      // lift one, preferentially one that isn't a constant if there was an
      // instruction that fit constant This first part checks whether there are
      // arguments (one or both) that have matched not a single instruction. In
      // this case, we have to lift them anyway, because otherwise they will
      // keep on not-matching.
      changed = false;
      if (!argHasMatched[0]) {
        liftArgLambda(0U, true);
      }
      if (argsAreEqual && changed) {
        continue;
      }
      if (!argHasMatched[1]) {
        liftArgLambda(1U);
      }
      if (changed) {
        continue; // If at least one of the arguments was lifted, we continue
      }

      if ((!argHasBeenLifted[0]) && (inputStorages[1].type == StorageType::CONSTANT)) {
        liftArgLambda(0U);
      } else if ((!argHasBeenLifted[1]) && (inputStorages[0].type == StorageType::CONSTANT)) {
        liftArgLambda(1U);
      } else if ((!argHasBeenLifted[0]) && (!startedAsWritableScratchReg[0])) {
        liftArgLambda(0U, true);
      } else if ((!argHasBeenLifted[1]) && (!startedAsWritableScratchReg[1])) {
        liftArgLambda(1U, true);
      } else {
        static_cast<void>(0);
      }
    }
  }
  // GCOVR_EXCL_START
  UNREACHABLE(_, "Instruction selection error will be first to catch");
  // GCOVR_EXCL_STOP
}

void Assembler::patchInstructionAtOffset(MemWriter &binary, uint32_t const offset, FunctionRef<void(Instruction &instruction)> const &lambda) {
  uint8_t *const patchPtr{binary.posToPtr(offset)};
  OPCodeTemplate const opTemplate{readFromPtr<OPCodeTemplate>(patchPtr)};
  Instruction instruction{Instruction(opTemplate, binary).setEmitted()};
  lambda(instruction);
  writeToPtr<OPCodeTemplate>(patchPtr, instruction.getOPCode());
}

RelPatchObj Assembler::prepareJMP(CC const conditionCode) const {
  uint32_t const position{binary_.size()};

  if (conditionCode == CC::NONE) {
    INSTR(B_imm26sxls2_t).setImm19o26ls2BranchPlaceHolder()();
  } else {
    INSTR(Bcondl_imm19sxls2_t).setCond(true, conditionCode).setImm19o26ls2BranchPlaceHolder()();
  }
  return RelPatchObj(position, binary_);
}

RelPatchObj Assembler::prepareJMPIfRegIsZero(REG const reg, bool const is64) const {
  uint32_t const position{binary_.size()};

  OPCodeTemplate const instr{is64 ? CBZ_xT_imm19sxls2_t : CBZ_wT_imm19sxls2_t};
  INSTR(instr).setT(reg).setImm19o26ls2BranchPlaceHolder()();
  return RelPatchObj(position, binary_, false);
}

RelPatchObj Assembler::prepareJMPIfRegIsNotZero(REG const reg, bool const is64) const {
  uint32_t const position{binary_.size()};

  OPCodeTemplate const instr{is64 ? CBNZ_xT_imm19sxls2_t : CBNZ_wT_imm19sxls2_t};
  INSTR(instr).setT(reg).setImm19o26ls2BranchPlaceHolder()();
  return RelPatchObj(position, binary_, false);
}

RelPatchObj Assembler::prepareADR(REG const targetReg) const {
  uint32_t const position{binary_.size()};

  INSTR(ADR_xD_signedOffset21_t).setD(targetReg).setSigned21AddressOffset(SafeInt<21>::fromConst<0x00>())();
  return RelPatchObj(position, binary_, false);
}

MachineType Assembler::getMachineTypeFromArgType(ArgType const argType) VB_NOEXCEPT {
  // coverity[autosar_cpp14_a7_2_1_violation]
  ArgType const type{static_cast<ArgType>(static_cast<uint8_t>(argType) & static_cast<uint8_t>(ArgType::TYPEMASK))};
  if (type == ArgType::I32) {
    return MachineType::I32;
  }
  if (type == ArgType::I64) {
    return MachineType::I64;
  }
  if (type == ArgType::F32) {
    return MachineType::F32;
  }
  if (type == ArgType::F64) {
    return MachineType::F64;
  }
  return MachineType::INVALID;
}

} // namespace aarch64
} // namespace vb
#endif

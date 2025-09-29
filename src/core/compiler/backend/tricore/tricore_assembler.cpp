///
/// @file tricore_assembler.cpp
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
#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>

#include "tricore_assembler.hpp"
#include "tricore_backend.hpp"
#include "tricore_cc.hpp"

#include "src/core/common/FunctionRef.hpp"
#include "src/core/common/Span.hpp"
#include "src/core/common/TrapCode.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/basedataoffsets.hpp"
#include "src/core/common/implementationlimits.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/Compiler.hpp"
#include "src/core/compiler/backend/tricore/tricore_cc.hpp"
#include "src/core/compiler/backend/tricore/tricore_encoding.hpp"
#include "src/core/compiler/backend/tricore/tricore_instruction.hpp"
#include "src/core/compiler/backend/tricore/tricore_relpatchobj.hpp"
#include "src/core/compiler/common/Common.hpp"
#include "src/core/compiler/common/MachineType.hpp"
#include "src/core/compiler/common/MemWriter.hpp"
#include "src/core/compiler/common/ModuleInfo.hpp"
#include "src/core/compiler/common/RegMask.hpp"
#include "src/core/compiler/common/SafeInt.hpp"
#include "src/core/compiler/common/StackElement.hpp"
#include "src/core/compiler/common/StackType.hpp"
#include "src/core/compiler/common/VariableStorage.hpp"
#include "src/core/compiler/common/util.hpp"

namespace vb {
namespace tc {
using Assembler = Tricore_Assembler; ///< Shortcut for Tricore_Assembler

namespace BD = Basedata;

Tricore_Assembler::Tricore_Assembler(Tricore_Backend &backend, MemWriter &binary, ModuleInfo &moduleInfo) VB_NOEXCEPT : backend_(backend),
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

Instruction Assembler::INSTR(AbstrInstr const &abstrInstr) const VB_NOEXCEPT {
#if ENABLE_EXTENSIONS
  if (backend_.compiler_.getDwarfGenerator() != nullptr) {
    backend_.compiler_.getDwarfGenerator()->record(binary_.size());
  }
#endif
  return Instruction(abstrInstr.opcode, binary_);
}

Assembler::PreparedArgs Assembler::loadArgsToRegsAndPrepDest(MachineType const dstType, StackElement const *const arg0,
                                                             StackElement const *const arg1, StackElement const *const targetHint,
                                                             RegMask const protRegs, bool const forceDstArg0Diff, bool const forceDstArg1Diff) const {
  bool const unop{arg1 == nullptr};
  assert((arg0 != nullptr) && "First source cannot be undefined");

  bool const noDest{dstType == MachineType::INVALID};

  // coverity[autosar_cpp14_a8_5_2_violation]
  const auto srcTypes = make_array(moduleInfo_.getMachineType(arg0), moduleInfo_.getMachineType(arg1));

  REG const verifiedTargetHintReg{backend_.getUnderlyingRegIfSuitable(targetHint, dstType, protRegs)};
  StackElement const *const verifiedTargetHint{(verifiedTargetHintReg != REG::NONE) ? targetHint : nullptr};

  std::array<bool const, 2> const startedAsWritableScratchReg{{backend_.isWritableScratchReg(arg0), backend_.isWritableScratchReg(arg1)}};
  std::array<bool, 2> argCanBeDst{};

  argCanBeDst[0] = startedAsWritableScratchReg[0] || backend_.common_.inSameReg(arg0, verifiedTargetHint, true);
  argCanBeDst[1] = startedAsWritableScratchReg[1] || backend_.common_.inSameReg(arg1, verifiedTargetHint, true);

  constexpr StackElement invalidElem{StackElement::invalid()};
  std::array<StackElement, 2> inputArgs{{(arg0 != nullptr) ? *arg0 : invalidElem, (arg1 != nullptr) ? *arg1 : invalidElem}};

  // Check whether both are equal to another and not INVALID
  bool const argsAreEqual{StackElement::equalsVariable(&inputArgs[0], &inputArgs[1])};

  // Lambda functions that can be used to lift the arguments
  std::array<REG, 2> argRegs{{REG::NONE, REG::NONE}};
  // coverity[autosar_cpp14_a8_5_2_violation]
  auto liftArgLambda = [this, &inputArgs, verifiedTargetHint, &argCanBeDst, argsAreEqual, protRegs, &argRegs](uint32_t const idx) mutable {
    assert((argRegs[idx] == REG::NONE) && "Cannot lift arg twice");
    assert((!protRegs.allMarked()) && "Cannot lift");
    assert(idx <= 1U && "Lift index out of range"); // As we only have two args, idx must be 0 or 1

    // otherIdx is 1 if idx is 0, else otherIdx is 0
    uint32_t const otherIdx{idx ^ 1U};
    if (argsAreEqual && (argRegs[otherIdx] != REG::NONE)) {
      inputArgs[idx] = inputArgs[otherIdx];
      argRegs[idx] = argRegs[otherIdx];
    } else {
      RegAllocTracker regAllocTracker{};
      regAllocTracker.writeProtRegs = protRegs | backend_.mask(&inputArgs[otherIdx]);
      argRegs[idx] = backend_.common_.liftToRegInPlaceProt(inputArgs[idx], true, verifiedTargetHint, regAllocTracker).reg;
    }

    // Lifted arg can now be dest, as it's now guaranteed to be in a writable register
    argCanBeDst[idx] = true;

    // If both args are equal, set the other arg to the newly lifted one and
    // also set argCanBeDst accordingly
    if (argsAreEqual && (argRegs[otherIdx] == REG::NONE)) {
      inputArgs[otherIdx] = inputArgs[idx];
      argCanBeDst[otherIdx] = true;
      argRegs[otherIdx] = argRegs[idx];
    }
  };

  // Lift arguments to registers

  VariableStorage const arg0Storage{moduleInfo_.getStorage(*arg0)}; // NOLINT(clang-analyzer-core.NonNullParamChecker)
  if (arg0Storage.type != StorageType::REGISTER) {
    liftArgLambda(0U);
  } else {
    argRegs[0] = arg0Storage.location.reg;
  }

  if ((!unop) && (argRegs[1] == REG::NONE)) {
    VariableStorage const arg1Storage{moduleInfo_.getStorage(*arg1)}; // NOLINT(clang-analyzer-core.NonNullParamChecker)
    if (arg1Storage.type != StorageType::REGISTER) {
      liftArgLambda(1U);
    } else {
      argRegs[1] = arg1Storage.location.reg;
    }
  }

  RegElement dstRegElem{RegElement{StackElement::invalid(), REG::NONE}};
  if (!noDest) {
    bool canUseTargetHintAsDst{false};
    if (verifiedTargetHint != nullptr) {
      // coverity[autosar_cpp14_a8_5_2_violation]
      auto const isArgStoragePartOfTargetHint = [this, &verifiedTargetHint, verifiedTargetHintReg](StackElement const *const arg)
                                                    VB_NOEXCEPT -> bool {
        assert(verifiedTargetHintReg != REG::NONE);
        if (arg == nullptr) {
          return false;
        }

        MachineType const targetHintType{moduleInfo_.getMachineType(verifiedTargetHint)};

        VariableStorage const argStorage{moduleInfo_.getStorage(*arg)};
        if (argStorage.type != StorageType::REGISTER) {
          return false;
        }

        // Both are regs, if targetHint is verified it's definitely in a register too
        assert(verifiedTargetHintReg != REG::NONE);

        REG const argReg{argStorage.location.reg};
        if (argReg == verifiedTargetHintReg) {
          return true;
        }

        if (MachineTypeUtil::getSize(targetHintType) != MachineTypeUtil::getSize(argStorage.machineType)) {
          // One must be 64-bit, the other 32-bit
          if (argStorage.type == StorageType::REGISTER) {
            REG simpleReg;
            REG extendedReg;
            if (MachineTypeUtil::is64(targetHintType)) {
              simpleReg = argStorage.location.reg;
              extendedReg = verifiedTargetHintReg;
            } else {
              simpleReg = verifiedTargetHintReg;
              extendedReg = argStorage.location.reg;
            }
            assert(RegUtil::canBeExtReg(extendedReg));
            if ((simpleReg == extendedReg) || (simpleReg == RegUtil::getOtherExtReg(simpleReg))) {
              return true;
            }
          }
        }
        return false;
      };

      canUseTargetHintAsDst = (!(forceDstArg0Diff && isArgStoragePartOfTargetHint(&inputArgs[0]))) &&
                              (!(forceDstArg1Diff && isArgStoragePartOfTargetHint(&inputArgs[1])));
    }
    if (canUseTargetHintAsDst) {
      // coverity[autosar_cpp14_m0_1_9_violation]
      dstRegElem = RegElement{*verifiedTargetHint, verifiedTargetHintReg};
    } else if (((!forceDstArg0Diff) && argCanBeDst[0]) && (srcTypes[0] == dstType)) {
      dstRegElem = RegElement{inputArgs[0], argRegs[0]};
    } else if (((!forceDstArg1Diff) && argCanBeDst[1]) && (srcTypes[1] == dstType)) {
      // coverity[autosar_cpp14_m0_1_9_violation]
      dstRegElem = RegElement{inputArgs[1], argRegs[1]};
    } else {
      assert(!canUseTargetHintAsDst && "Cannot use targetHint, otherwise canUseTargetHintAsDst would be true");
      RegMask const targetHintMask{(verifiedTargetHint != nullptr) ? backend_.mask(verifiedTargetHint) : RegMask::none()};
      RegAllocTracker fullRegAllocTracker{};
      fullRegAllocTracker.readProtRegs = protRegs | backend_.mask(&inputArgs[0]) | backend_.mask(&inputArgs[1]) | targetHintMask;
      dstRegElem = backend_.common_.reqScratchRegProt(dstType, fullRegAllocTracker, false);
    }

    assert((!forceDstArg0Diff || !StackElement::equalsVariable(&dstRegElem.elem, &inputArgs[0])) && "Error, used forbidden arg as dest");
    assert((!forceDstArg1Diff || !StackElement::equalsVariable(&dstRegElem.elem, &inputArgs[1])) && "Error, used forbidden arg as dest");
  }
  // coverity[autosar_cpp14_a16_2_3_violation]
  PreparedArgs preparedArgs{};
  preparedArgs.dest.elem = backend_.common_.getResultStackElement(&dstRegElem.elem, dstType);
  preparedArgs.dest.reg = dstRegElem.reg;
  preparedArgs.dest.secReg = (noDest || (MachineTypeUtil::getSize(dstType) == 4U)) ? REG::NONE : RegUtil::getOtherExtReg(preparedArgs.dest.reg);
  preparedArgs.arg0.elem = inputArgs[0];
  preparedArgs.arg0.reg = argRegs[0];
  preparedArgs.arg0.secReg = (MachineTypeUtil::getSize(srcTypes[0]) == 4U) ? REG::NONE : RegUtil::getOtherExtReg(preparedArgs.arg0.reg);
  if (arg1 != nullptr) {
    preparedArgs.arg1.elem = inputArgs[1];
    preparedArgs.arg1.reg = argRegs[1];
    preparedArgs.arg1.secReg = (MachineTypeUtil::getSize(srcTypes[1]) == 4U) ? REG::NONE : RegUtil::getOtherExtReg(preparedArgs.arg1.reg);
  }
  return preparedArgs;
}

StackElement Assembler::selectInstr(Span<AbstrInstr const> const &instructions, StackElement const *const arg0, StackElement const *const arg1,
                                    StackElement const *const targetHint, RegMask const protRegs) VB_THROW {
  assert(instructions.size() > 0 && "Zero instructions to select from");
  assert((arg0 != nullptr) && "First source cannot be undefined");

  bool const unop{arg1 == nullptr};
  bool const src_0_1_commutative{instructions[0].src_0_1_commutative};
  static_cast<void>(src_0_1_commutative);
  assert((!unop || !src_0_1_commutative) && "Unary operation cannot be commutative");

  MachineType const dstType{getMachineTypeFromArgType(instructions[0].destType)};
  REG const verifiedTargetHintReg{backend_.getUnderlyingRegIfSuitable(targetHint, dstType, protRegs)};
  StackElement const *const verifiedTargetHint{(verifiedTargetHintReg != REG::NONE) ? targetHint : nullptr};

  constexpr StackElement invalidElem{StackElement::invalid()};
  std::array<StackElement, 2U> inputArgs{{(arg0 != nullptr) ? *arg0 : invalidElem, (arg1 != nullptr) ? *arg1 : invalidElem}};

  // Check whether both are equal to another and not INVALID
  // bool const argsAreEqual{StackElement::equalsVariable(&inputArgs[0], &inputArgs[1])};
  bool const argsAreEqual{moduleInfo_.getStorage(inputArgs[0]).inSameLocation(moduleInfo_.getStorage(inputArgs[1]))};
  std::array<bool, 2> argHasBeenLifted{{false, false}};
  bool const isD15Available{backend_.isD15Available()};

  // coverity[autosar_cpp14_a8_5_2_violation]
  auto liftArgLambda = [this, &inputArgs, verifiedTargetHint, argsAreEqual, protRegs, isD15Available,
                        &argHasBeenLifted](uint32_t const idx, bool const coLift, bool const liftToD15) mutable {
    assert(!argHasBeenLifted[idx] && "Cannot lift arg twice");
    assert((!protRegs.allMarked()) && "Cannot lift");
    assert(idx <= 1U && "Lift index out of range"); // As we only have two args, idx must be 0 or 1

    // otherIdx is 1 if idx is 0, else otherIdx is 0
    uint32_t const otherIdx{idx ^ 1U};
    if ((argsAreEqual && argHasBeenLifted[otherIdx]) && coLift) {
      inputArgs[idx] = inputArgs[otherIdx];
    } else {
      RegAllocTracker tempRegAllocTracker{};
      tempRegAllocTracker.writeProtRegs = protRegs | backend_.mask(&inputArgs[otherIdx]);
      StackElement const d15{StackElement::scratchReg(REG::D15, MachineTypeUtil::toStackTypeFlag(MachineType::I32))};
      StackElement const *target{nullptr};
      if (liftToD15) {
        target = &d15;
      } else if ((verifiedTargetHint == nullptr) && isD15Available) {
        target = &d15;
      } else {
        target = verifiedTargetHint;
      }
      StackElement const scratchReg{
          backend_.common_.reqScratchRegProt(moduleInfo_.getMachineType(&inputArgs[idx]), target, tempRegAllocTracker, false).elem};
      VariableStorage const srcStorage{moduleInfo_.getStorage(inputArgs[idx])};
      VariableStorage dstStorage{moduleInfo_.getStorage(scratchReg)};
      if (srcStorage.machineType != dstStorage.machineType) {
        dstStorage.machineType = srcStorage.machineType;
      }
      backend_.emitMoveImpl(dstStorage, srcStorage, false);
      inputArgs[idx] = scratchReg;
    }

    // If both args are equal, set the other arg to the newly lifted one and
    // also set argCanBeDst accordingly
    if ((argsAreEqual && !argHasBeenLifted[otherIdx]) && coLift) {
      inputArgs[otherIdx] = inputArgs[idx];
      argHasBeenLifted[otherIdx] = true;
    }
  };

  // prelift args (in memory) to reg
  if (moduleInfo_.getStorage(inputArgs[0]).inMemory()) {
    liftArgLambda(0U, true, false);
  }
  if ((!unop) && (!argHasBeenLifted[1])) {
    if (moduleInfo_.getStorage(inputArgs[1]).inMemory()) {
      liftArgLambda(1U, true, false);
    }
  }

  // Whether arg0, arg1, dest is located in writable scratch register
  std::array<bool const, 3> const startedAsWritableScratchReg{{backend_.isWritableScratchReg(&inputArgs[0]),
                                                               backend_.isWritableScratchReg(&inputArgs[1]),
                                                               backend_.isWritableScratchReg(verifiedTargetHint)}};
  OperandMovement minCostMovement{invalidMovCost, 0U, false, false, false};
  uint32_t selectedInstrIdx{UINT32_MAX};
  VariableStorage const destStorage{(verifiedTargetHint != nullptr) ? moduleInfo_.getStorage(*verifiedTargetHint) : VariableStorage{}};
  for (uint32_t instrIdx{0U}; instrIdx < instructions.size(); instrIdx++) {
    AbstrInstr const abstrInstr{instructions[instrIdx]};
    OperandMovement const cost{getInstructionCost(abstrInstr, moduleInfo_.getStorage(inputArgs[0]), moduleInfo_.getStorage(inputArgs[1]),
                                                  startedAsWritableScratchReg, destStorage, isD15Available)};
    if ((cost.cost < minCostMovement.cost) || ((cost.cost == minCostMovement.cost) && (cost.liftCount < minCostMovement.liftCount))) {
      selectedInstrIdx = instrIdx;
      minCostMovement = cost;
    }
  }

  assert((selectedInstrIdx != UINT32_MAX) && "Instruction selection error will be first to catch");

  // coverity[autosar_cpp14_a5_2_5_violation]
  AbstrInstr const selectedInstruction{instructions[selectedInstrIdx]};
  if (minCostMovement.movArg0) {
    ArgType const arg0Type{minCostMovement.reversed ? selectedInstruction.src1Type : selectedInstruction.src0Type};
    liftArgLambda(0U, minCostMovement.movArg1, (arg0Type == ArgType::d15));
  }
  if (minCostMovement.movArg1 && !argHasBeenLifted[1]) {
    ArgType const arg1Type{minCostMovement.reversed ? selectedInstruction.src0Type : selectedInstruction.src1Type};
    liftArgLambda(1U, false, (arg1Type == ArgType::d15));
  }

  std::array<VariableStorage, 2U> inputStorages{{
      (arg0 != nullptr) ? moduleInfo_.getStorage(inputArgs[0]) : VariableStorage{},
      (arg1 != nullptr) ? moduleInfo_.getStorage(inputArgs[1]) : VariableStorage{},
  }};
  bool const noDest{instructions[0].destType == ArgType::NONE};
  VariableStorage dest{};
  if (!noDest) {
    if (verifiedTargetHint != nullptr) {
      dest = VariableStorage::reg(verifiedTargetHintReg, dstType);
    } else if (backend_.isWritableScratchReg(&inputArgs[0])) {
      dest = inputStorages[0];
    } else if (backend_.isWritableScratchReg(&inputArgs[1])) {
      dest = inputStorages[1];
    } else {
      RegAllocTracker fullRegAllocTracker{};
      fullRegAllocTracker.readProtRegs = protRegs | backend_.mask(inputStorages[0]) | backend_.mask(inputStorages[1]);
      StackElement const d15{StackElement::scratchReg(REG::D15, MachineTypeUtil::toStackTypeFlag(MachineType::I32))};
      StackElement const *const target{(selectedInstruction.destType == ArgType::d15) ? &d15 : nullptr};
      RegElement const regElement{backend_.common_.reqScratchRegProt(dstType, target, fullRegAllocTracker, false)};
      dest = VariableStorage::reg(regElement.reg, dstType);
    }
  }

  if (minCostMovement.reversed) {
    emitAbstrInstr(selectedInstruction, dest, inputStorages[1], inputStorages[0]);
  } else {
    emitAbstrInstr(selectedInstruction, dest, inputStorages[0], inputStorages[1]);
  }

  StackElement result{StackElement::invalid()};
  if ((targetHint != nullptr) && dest.inSameLocation(moduleInfo_.getStorage(*targetHint))) {
    result = backend_.common_.getResultStackElement(targetHint, dstType);
  } else {
    if (!noDest) {
      assert(dest.type == StorageType::REGISTER && "Invalid storage type");
      result = StackElement::scratchReg(dest.location.reg, MachineTypeUtil::toStackTypeFlag(dstType));
    }
  }
  return result;
}

void Assembler::setStackFrameSize(uint32_t const frameSize, bool const temporary, bool const mayRemoveLocals, uint32_t const functionEntryAdjust) {
  assert((frameSize == moduleInfo_.getStackFrameSizeBeforeReturn()) || frameSize == alignStackFrameSize(frameSize));
  assert(frameSize >= moduleInfo_.getStackFrameSizeBeforeReturn() && "Cannot remove return address and parameters");

  if (!mayRemoveLocals) {
    assert(frameSize >= moduleInfo_.getFixedStackFrameWidth() && "Cannot implicitly drop active variables (tempstack, local) by truncating stack");
  }

  if (moduleInfo_.fnc.stackFrameSize != frameSize) {
    constexpr uint32_t maxAllowedStackFrameSize{UINT32_MAX};
    static_assert(maxAllowedStackFrameSize >= ImplementationLimits::maxStackFrameSize, "Maximum stack frame size too large");
    if (frameSize > ImplementationLimits::maxStackFrameSize) {
      throw ImplementationLimitationException(ErrorCode::Reached_maximum_stack_frame_size);
    }

    if (moduleInfo_.fnc.stackFrameSize > frameSize) {
      addImmToReg(REG::SP, moduleInfo_.fnc.stackFrameSize - frameSize);
    } else /* frameSize > moduleInfo_.fnc.stackFrameSize */ {
      subSp((frameSize - moduleInfo_.fnc.stackFrameSize) + functionEntryAdjust);
    }

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

void Assembler::addImmToReg(REG const reg, uint32_t const imm, REG targetReg) const {
  assert(((targetReg == REG::NONE) || (RegUtil::isDATA(reg) == RegUtil::isDATA(targetReg))) && "Reg and targetReg need to be of the same type");

  if (targetReg == REG::NONE) {
    targetReg = reg;
  }

  if (imm == 0U) {
    if (targetReg == reg) {
      return;
    }
    if (RegUtil::isDATA(reg)) {
      INSTR(MOV_Da_Db).setDa(targetReg).setDb(reg)();
      return;
    }
    INSTR(MOVAA_Aa_Ab).setAa(targetReg).setAb(reg)();
    return;
  }

  REG sourceReg{reg};
  if (RegUtil::isDATA(reg)) {
    SignedInRangeCheck<4U> const const4sxChecker{SignedInRangeCheck<4U>::check(bit_cast<int32_t>(imm))};
    if (const4sxChecker.inRange()) {
      if (targetReg == reg) {
        INSTR(ADD_Da_const4sx).setDa(reg).setConst4sx(const4sxChecker.safeInt())();
        return;
      } else if (reg == REG::D15) {
        INSTR(ADD_Da_D15_const4sx).setDa(targetReg).setConst4sx(const4sxChecker.safeInt())();
        return;
      } else if (targetReg == REG::D15) {
        INSTR(ADD_D15_Da_const4sx).setDa(reg).setConst4sx(const4sxChecker.safeInt())();
        return;
      } else {
        static_cast<void>(0);
      }
    }
    if ((imm & 0xFFFFU) != 0U) {
      INSTR(ADDI_Dc_Da_const16sx).setDc(targetReg).setDa(sourceReg).setConst16sx(Instruction::lower16sx(imm))();
      sourceReg = targetReg;
    }
    SafeUInt<16U> const reducedHighPortionToAdd{SafeUInt<32U>::fromAny(imm + 0x8000U).rightShift<16U>()};
    if (reducedHighPortionToAdd.value() != 0U) {
      INSTR(ADDIH_Dc_Da_const16).setDc(targetReg).setDa(sourceReg).setConst16(reducedHighPortionToAdd)();
    }
    return;
  }
  // address register
  SignedInRangeCheck<4> const inRangeCheck{SignedInRangeCheck<4>::check(static_cast<int32_t>(imm))};
  if (inRangeCheck.inRange() && (sourceReg == targetReg)) {
    INSTR(ADDA_Aa_const4sx).setAa(targetReg).setConst4sx(inRangeCheck.safeInt())();
    return;
  }
  if ((imm & 0xFFFFU) != 0U) {
    INSTR(LEA_Aa_deref_Ab_off16sx).setAa(targetReg).setAb(sourceReg).setOff16sx(Instruction::lower16sx(imm))();
    sourceReg = targetReg;
  }
  SafeUInt<16U> const reducedHighPortionToAdd{SafeUInt<32U>::fromAny(imm + 0x8000U).rightShift<16U>()};
  if (reducedHighPortionToAdd.value() != 0U) {
    INSTR(ADDIHA_Ac_Aa_const16).setAc(targetReg).setAa(sourceReg).setConst16(reducedHighPortionToAdd)();
  }
}

void Assembler::subSp(uint32_t const imm) const {
  if (imm == 0U) {
    return;
  }
  UnsignedInRangeCheck<8U> const rangeCheck{UnsignedInRangeCheck<8U>::check(imm)};
  if (rangeCheck.inRange()) {
    INSTR(SUBA_A10_const8zx).setConst8zx(rangeCheck.safeInt())();
  } else {
    addImmToReg(REG::SP, 0U - imm);
  }
}

void Assembler::MOVimm(REG const reg, uint32_t const imm) const {
  if (RegUtil::isDATA(reg)) {
    SignedInRangeCheck<4U> const const4sxChecker{SignedInRangeCheck<4U>::check(bit_cast<int32_t>(imm))};
    if (const4sxChecker.inRange()) {
      INSTR(MOV_Da_const4sx).setDa(reg).setConst4sx(const4sxChecker.safeInt())();
    } else if ((reg == REG::D15) && UnsignedInRangeCheck<8U>::check(imm).inRange()) {
      INSTR(MOV_D15_const8zx).setConst8zx(SafeUInt<8U>::fromUnsafe(imm))();
    } else if (UnsignedInRangeCheck<16U>::check(imm).inRange()) {
      INSTR(MOVU_Dc_const16zx).setDc(reg).setConst16zx(SafeUInt<16U>::fromUnsafe(imm))();
    } else if ((imm & 0xFFFFU) == 0U) {
      INSTR(MOVH_Dc_const16).setDc(reg).setConst16(SafeUInt<32U>::fromAny(imm).rightShift<16U>())();
    } else {
      // TODO(Xinquan): can be optimized for lower16sx
      INSTR(MOV_Dc_const16sx).setDc(reg).setConst16sx(Instruction::lower16sx(imm))();

      SafeUInt<16U> const reducedHighPortionToAdd{SafeUInt<32U>::fromAny(imm + 0x8000U).rightShift<16U>()};
      if (reducedHighPortionToAdd.value() != 0U) {
        INSTR(ADDIH_Dc_Da_const16).setDc(reg).setDa(reg).setConst16(reducedHighPortionToAdd)();
      }
    }
  } else {
    UnsignedInRangeCheck<4U> const rangeCheck{UnsignedInRangeCheck<4U>::check(imm)};
    if (rangeCheck.inRange()) {
      INSTR(MOVA_Aa_const4zx).setAa(reg).setConst4zx(rangeCheck.safeInt())();
    } else {
      SafeUInt<16U> const reducedHighPortion{SafeUInt<32U>::fromAny(imm + 0x8000U).rightShift<16U>()};
      INSTR(MOVHA_Ac_const16).setAc(reg).setConst16(reducedHighPortion)();
      if ((imm & 0xFFFFU) != 0U) {
        INSTR(LEA_Aa_deref_Ab_off16sx).setAa(reg).setAb(reg).setOff16sx(Instruction::lower16sx(imm))();
      }
    }
  }
}

void Assembler::loadWordDRegDerefARegDisp16sx(REG const dataReg, REG const addrReg, SafeInt<16U> const disp) const {
  bool const dispGreaterEqualThan0{disp.value() >= 0};
  bool const dispMod4Equal0{dispGreaterEqualThan0 && ((disp.value() % 4) == 0)};
  UnsignedInRangeCheck<10> const rangeCheck10{UnsignedInRangeCheck<10>::check(static_cast<uint32_t>(disp.value()))};
  UnsignedInRangeCheck<6> const rangeCheck6{UnsignedInRangeCheck<6>::check(static_cast<uint32_t>(disp.value()))};
  if (disp.value() == 0) {
    INSTR(LDW_Dc_deref_Ab).setDc(dataReg).setAb(addrReg)();
  } else if (((dataReg == REG::D15) && (addrReg == REG::A10)) && (rangeCheck10.inRange() && dispMod4Equal0)) {
    INSTR(LDW_D15_deref_A10_const8zxls2).setConst8zxls2(rangeCheck10.safeInt())();
  } else if ((dataReg == REG::D15) && (rangeCheck6.inRange() && dispMod4Equal0)) {
    INSTR(LDW_D15_deref_Ab_off4srozxls2).setAb(addrReg).setOff4srozxls2(rangeCheck6.safeInt())();
  } else if ((addrReg == REG::A15) && (rangeCheck6.inRange() && dispMod4Equal0)) {
    INSTR(LDW_Dc_deref_A15_off4zxls2).setDc(addrReg).setOff4zxls2(rangeCheck6.safeInt())();
  } else {
    INSTR(LDW_Da_deref_Ab_off16sx).setDa(dataReg).setAb(addrReg).setOff16sx(disp)();
  }
}

void Assembler::loadByteUnsignedDRegDerefARegDisp16sx(REG const dataReg, REG const addrReg, SafeInt<16U> const disp) const {
  bool const dispGreaterEqualThan0{disp.value() >= 0};
  UnsignedInRangeCheck<4> const rangeCheck4{UnsignedInRangeCheck<4>::check(static_cast<uint32_t>(disp.value()))};
  if (disp.value() == 0) {
    INSTR(LDBU_Dc_deref_Ab).setDc(dataReg).setAb(addrReg)();
  } else if ((dataReg == REG::D15) && ((dispGreaterEqualThan0 && rangeCheck4.inRange()))) {
    INSTR(LDBU_D15_deref_Ab_off4srozx).setAb(addrReg).setOff4srozx(rangeCheck4.safeInt())();
  } else if ((addrReg == REG::A15) && ((dispGreaterEqualThan0 && rangeCheck4.inRange()))) {
    INSTR(LDBU_Dc_deref_A15_off4zx).setDc(dataReg).setOff4zx(rangeCheck4.safeInt())();
  } else {
    INSTR(LDBU_Da_deref_Ab_off16sx).setDa(dataReg).setAb(addrReg).setOff16sx(disp)();
  }
}

void Assembler::loadHalfwordDRegDerefARegDisp16sx(REG const dataReg, REG const addrReg, SafeInt<16U> const disp) const {
  bool const dispGreaterEqualThan0{disp.value() >= 0};
  bool const dispMod2Equal0{dispGreaterEqualThan0 && ((disp.value() % 2) == 0)};
  UnsignedInRangeCheck<5> const rangeCheck5{UnsignedInRangeCheck<5>::check(static_cast<uint32_t>(disp.value()))};
  if (disp.value() == 0) {
    INSTR(LDH_Dc_deref_Ab).setDc(dataReg).setAb(addrReg)();
  } else if ((dataReg == REG::D15) && (rangeCheck5.inRange() && dispMod2Equal0)) {
    INSTR(LDH_D15_deref_Ab_off4srozxls1).setAb(addrReg).setOff4srozxls1(rangeCheck5.safeInt())();
  } else if ((addrReg == REG::A15) && (rangeCheck5.inRange() && dispMod2Equal0)) {
    INSTR(LDH_Dc_deref_A15_off4zxls1).setDc(dataReg).setOff4zxls1(rangeCheck5.safeInt())();
  } else {
    INSTR(LDH_Da_deref_Ab_off16sx).setDa(dataReg).setAb(addrReg).setOff16sx(disp)();
  }
}

void Assembler::storeByteDerefARegDisp16sxDReg(REG const dataReg, REG const addrReg, SafeInt<16U> const disp) const {
  bool const dispGreaterEqualThan0{disp.value() >= 0};
  UnsignedInRangeCheck<4> const rangeCheck4{UnsignedInRangeCheck<4>::check(static_cast<uint32_t>(disp.value()))};
  if (disp.value() == 0) {
    INSTR(STB_deref_Ab_Da).setAb(addrReg).setDa(dataReg)();
  } else if ((dataReg == REG::D15) && ((dispGreaterEqualThan0 && rangeCheck4.inRange()))) {
    INSTR(STB_deref_Ab_off4srozx_D15).setAb(addrReg).setOff4srozx(rangeCheck4.safeInt())();
  } else if ((addrReg == REG::A15) && ((dispGreaterEqualThan0 && rangeCheck4.inRange()))) {
    INSTR(STB_deref_A15_off4zx_Da).setDa(dataReg).setOff4zx(rangeCheck4.safeInt())();
  } else {
    INSTR(STB_deref_Ab_off16sx_Da).setAb(addrReg).setDa(dataReg).setOff16sx(disp)();
  }
}

void Assembler::storeHalfwordDerefARegDisp16sxDReg(REG const dataReg, REG const addrReg, SafeInt<16U> const disp) const {
  bool const dispGreaterEqualThan0{disp.value() >= 0};
  bool const dispMod2Equal0{dispGreaterEqualThan0 && ((disp.value() % 2) == 0)};
  UnsignedInRangeCheck<5> const rangeCheck5{UnsignedInRangeCheck<5>::check(static_cast<uint32_t>(disp.value()))};
  if (disp.value() == 0) {
    INSTR(STH_deref_Ab_Da).setAb(addrReg).setDa(dataReg)();
  } else if ((dataReg == REG::D15) && (rangeCheck5.inRange() && dispMod2Equal0)) {
    INSTR(STH_deref_Ab_off4srozxls1_D15).setAb(addrReg).setOff4srozxls1(rangeCheck5.safeInt())();
  } else if ((addrReg == REG::A15) && (rangeCheck5.inRange() && dispMod2Equal0)) {
    INSTR(STH_deref_A15_off4zxls1_Da).setDa(dataReg).setOff4zxls1(rangeCheck5.safeInt())();
  } else {
    INSTR(STH_deref_Ab_off16sx_Da).setAb(addrReg).setDa(dataReg).setOff16sx(disp)();
  }
}

void Assembler::storeWordDerefARegDisp16sxDReg(REG const dataReg, REG const addrReg, SafeInt<16U> const disp) const {
  bool const dispGreaterEqualThan0{disp.value() >= 0};
  bool const dispMod4Equal0{dispGreaterEqualThan0 && ((disp.value() % 4) == 0)};
  UnsignedInRangeCheck<10> const rangeCheck10{UnsignedInRangeCheck<10>::check(static_cast<uint32_t>(disp.value()))};
  UnsignedInRangeCheck<6> const rangeCheck6{UnsignedInRangeCheck<6>::check(static_cast<uint32_t>(disp.value()))};
  if (disp.value() == 0) {
    INSTR(STW_deref_Ab_Da).setAb(addrReg).setDa(dataReg)();
  } else if (((dataReg == REG::D15) && (addrReg == REG::A10)) && (rangeCheck10.inRange() && dispMod4Equal0)) {
    INSTR(STW_deref_A10_const8zxls2_D15).setConst8zxls2(rangeCheck10.safeInt())();
  } else if ((dataReg == REG::D15) && (rangeCheck6.inRange() && dispMod4Equal0)) {
    INSTR(STW_deref_Ab_off4srozxls2_D15).setAb(addrReg).setOff4srozxls2(rangeCheck6.safeInt())();
  } else if ((addrReg == REG::A15) && (rangeCheck6.inRange() && dispMod4Equal0)) {
    INSTR(STW_deref_A15_off4zxls2_Da).setOff4zxls2(rangeCheck6.safeInt()).setDa(dataReg)();
  } else {
    INSTR(STW_deref_Ab_off16sx_Da).setAb(addrReg).setOff16sx(disp).setDa(dataReg)();
  }
}

void Assembler::patchInstructionAtOffset(MemWriter &binary, uint32_t const offset, FunctionRef<void(Instruction &instruction)> const &lambda) {
  uint8_t *const patchPtr{binary.posToPtr(offset)};
  OPCodeTemplate const opTemplate{readFromPtr<OPCodeTemplate>(patchPtr)};
  Instruction instruction{Instruction(opTemplate, binary).setEmitted()};
  lambda(instruction);
  writeToPtr<OPCodeTemplate>(patchPtr, instruction.getOPCode());
}

void Assembler::checkStackFence(REG const dataScrReg, REG const addrScrReg) const {
  assert(((dataScrReg != REG::NONE) && RegUtil::isDATA(dataScrReg)) && "Data scratch register needed");
  assert(((addrScrReg != REG::NONE) && !RegUtil::isDATA(addrScrReg)) && "Address scratch register needed");
  // if (stackFence >= $SP) trap;
  INSTR(LDA_Aa_deref_Ab_off16sx).setAa(addrScrReg).setAb(WasmABI::REGS::linMem).setOff16sx(SafeInt<16U>::fromConst<-BD::FromEnd::stackFence>())();
  INSTR(GEA_Dc_Aa_Ab).setDc(dataScrReg).setAa(addrScrReg).setAb(REG::SP)();
  cTRAP(TrapCode::STACKFENCEBREACHED, JumpCondition::bitTrue(dataScrReg, SafeInt<4U>::fromConst<0>()));
}

RelPatchObj Assembler::prepareJump(JumpCondition const &conditionJump) const {
  switch (conditionJump.getKind()) {
  case JumpCondition::Kind::bitFalse:
    return INSTR(JZT_Da_n_disp15sx2).setDa(conditionJump.getRegA()).setN(static_cast<SafeUInt<5U>>(conditionJump.getImm())).prepJmp();
  case JumpCondition::Kind::bitTrue:
    return INSTR(JNZT_Da_n_disp15sx2).setDa(conditionJump.getRegA()).setN(static_cast<SafeUInt<5U>>(conditionJump.getImm())).prepJmp();

  case JumpCondition::Kind::I32LtConst4sx:
    return INSTR(JLT_Da_const4sx_disp15sx2).setDa(conditionJump.getRegA()).setConst4sx(conditionJump.getImm()).prepJmp();
  case JumpCondition::Kind::I32GeConst4sx:
    return INSTR(JGE_Da_const4sx_disp15sx2).setDa(conditionJump.getRegA()).setConst4sx(conditionJump.getImm()).prepJmp();

  case JumpCondition::Kind::I32LtReg:
    return INSTR(JLT_Da_Db_disp15sx2).setDa(conditionJump.getRegA()).setDb(conditionJump.getRegB()).prepJmp();
  case JumpCondition::Kind::I32GeReg:
    return INSTR(JGE_Da_Db_disp15sx2).setDa(conditionJump.getRegA()).setDb(conditionJump.getRegB()).prepJmp();
  case JumpCondition::Kind::U32LtReg:
    return INSTR(JLTU_Da_Db_disp15sx2).setDa(conditionJump.getRegA()).setDb(conditionJump.getRegB()).prepJmp();
  case JumpCondition::Kind::U32GeReg:
    return INSTR(JGEU_Da_Db_disp15sx2).setDa(conditionJump.getRegA()).setDb(conditionJump.getRegB()).prepJmp();
  case JumpCondition::Kind::I32EqReg:
    return INSTR(JEQ_Da_Db_disp15sx2).setDa(conditionJump.getRegA()).setDb(conditionJump.getRegB()).prepJmp();
  case JumpCondition::Kind::I32NeReg:
    return INSTR(JNE_Da_Db_disp15sx2).setDa(conditionJump.getRegA()).setDb(conditionJump.getRegB()).prepJmp();

  case JumpCondition::Kind::AddrEqReg:
    return INSTR(JEQA_Aa_Ab_disp15sx2).setAa(conditionJump.getRegA()).setAb(conditionJump.getRegB()).prepJmp();
  case JumpCondition::Kind::AddrNeReg:
    return INSTR(JNEA_Aa_Ab_disp15sx2).setAa(conditionJump.getRegA()).setAb(conditionJump.getRegB()).prepJmp();

  case JumpCondition::Kind::I32EqConst4sx:
    return INSTR(JEQ_Da_const4sx_disp15sx2).setDa(conditionJump.getRegA()).setConst4sx(conditionJump.getImm()).prepJmp();
  case JumpCondition::Kind::I32NeConst4sx:
    return INSTR(JNE_Da_const4sx_disp15sx2).setDa(conditionJump.getRegA()).setConst4sx(conditionJump.getImm()).prepJmp();

  default:
    UNREACHABLE(return RelPatchObj{}, "missing instruction for conditional jump");
  }
}

void Assembler::TRAP(TrapCode const trapCode) const {
  uint32_t lastPosTrap{0U};
  SignedInRangeCheck<25U> const rangeCheckTrap{lastTrapPosition_.get<25U>(trapCode, binary_.size(), lastPosTrap)};
  if (rangeCheckTrap.inRange()) {
    // jump to the beginning of trap JIT code to save 2 instructions (For TrapCode::NONE is 1 instruction).
    INSTR(J_disp24sx2).setDisp24sx2(rangeCheckTrap.safeInt())();
    return;
  }
  if (trapCode != TrapCode::NONE) {
    // mov trapReg trapCode
    lastTrapPosition_.set(trapCode, binary_.size());
    MOVimm(WasmABI::REGS::trapReg, static_cast<uint32_t>(trapCode));
  }

  SignedInRangeCheck<25U> const rangeCheck{SignedInRangeCheck<25U>::check(
      static_cast<int32_t>(moduleInfo_.helperFunctionBinaryPositions.genericTrapHandler) - static_cast<int32_t>(binary_.size()))};
  uint32_t lastPosNone{0U};
  SignedInRangeCheck<25U> const rangeCheckNone{lastTrapPosition_.get<25U>(TrapCode::NONE, binary_.size(), lastPosNone)};
  if (rangeCheck.inRange()) {
    lastTrapPosition_.set(TrapCode::NONE, binary_.size());
    INSTR(J_disp24sx2).setDisp24sx2(rangeCheck.safeInt())();
  } else if (rangeCheckNone.inRange()) {
    INSTR(J_disp24sx2).setDisp24sx2(rangeCheckNone.safeInt())();
  } else {
    throw ImplementationLimitationException(ErrorCode::Branches_can_only_target_offsets_in_the_range___128MB);
  }
}

void Assembler::cTRAP(TrapCode const trapCode, JumpCondition const &conditionJump) const {
  uint32_t lastTrapJitCodePosition{0U};
  if (lastTrapPosition_.get<16U>(trapCode, binary_.size(), lastTrapJitCodePosition).inRange()) {
    // If the last trap JIT code can be reached, we can jump to last trap JIT code by conditional jump to reduce
    // instruction counts.
    prepareJump(conditionJump).linkToBinaryPos(lastTrapJitCodePosition);
    return;
  }
  RelPatchObj const jump{prepareJump(conditionJump.negateJump())};
  TRAP(trapCode);
  jump.linkToHere();
}

RelPatchObj Assembler::loadPCRelAddr(REG const addrTargetReg, REG const addrScratchReg) const {
  if (addrScratchReg != REG::NONE) {
    // Save current A11 because we will clobber it by loading a PC-relative address
    INSTR(MOVAA_Aa_Ab).setAa(addrScratchReg).setAb(REG::A11)();
  }

  // Move current PC (after instruction) to A11
  INSTR(JL_disp24sx2).setDisp24sx2(SafeInt<25>::fromConst<4>())();
  RelPatchObj const toTargetPC{
      INSTR(LEA_Aa_deref_Ab_off16sx).setAa(addrTargetReg).setAb(REG::A11).setOff16sx(SafeInt<16U>::fromConst<0>()).prepLEA()};

  if (addrScratchReg != REG::NONE) {
    // Restore A11
    INSTR(MOVAA_Aa_Ab).setAa(REG::A11).setAb(addrScratchReg)();
  }
  return toTargetPC;
}

void Assembler::emitDcDaDb(OPCodeTemplate const instruction16, OPCodeTemplate const instruction32, REG const Dc, REG const Da, REG const Db) const {
  if (Dc == REG::D15) {
    INSTR(instruction16).setDa(Da).setDb(Db)();
  } else {
    INSTR(instruction32).setDc(Dc).setDa(Da).setDb(Db)();
  }
}

void Assembler::emitDcDaConst9sx(OPCodeTemplate const instruction16, OPCodeTemplate const instruction32, REG const Dc, REG const Da,
                                 SafeInt<9> const const9) const {
  SignedInRangeCheck<4U> const rangeCheck{SignedInRangeCheck<4U>::check(const9.value())};
  if ((Dc == REG::D15) && rangeCheck.inRange()) {
    INSTR(instruction16).setDa(Da).setConst4sx(rangeCheck.safeInt())();
  } else {
    INSTR(instruction32).setDc(Dc).setDa(Da).setConst9sx(const9)();
  }
}

void Assembler::emitDcDaConst9zx(OPCodeTemplate const instruction16, OPCodeTemplate const instruction32, REG const Dc, REG const Da,
                                 SafeUInt<9> const const9) const {
  UnsignedInRangeCheck<8U> const rangeCheck{UnsignedInRangeCheck<8U>::check(const9.value())};
  if (((Dc == REG::D15) && (Da == REG::D15)) && rangeCheck.inRange()) {
    INSTR(instruction16).setConst8zx(rangeCheck.safeInt())();
  } else {
    INSTR(instruction32).setDc(Dc).setDa(Da).setConst9zx(const9)();
  }
}

void Assembler::emitLoadDerefOff16sx(REG const desDataReg, REG const addrBaseReg, SafeInt<16> const offset16) const {
  if (offset16.value() == static_cast<SafeInt<16>::ValueType>(0)) {
    INSTR(LDA_Ac_deref_Ab).setAc(desDataReg).setAb(addrBaseReg)();
  } else {
    INSTR(LDA_Aa_deref_Ab_off16sx).setAa(desDataReg).setAb(addrBaseReg).setOff16sx(offset16)();
  }
}

void Assembler::emitStoreDerefOff16sx(REG const addrBaseReg, REG const srcDataReg, SafeInt<16> const offset16) const {
  if (offset16.value() == static_cast<SafeInt<16>::ValueType>(0)) {
    INSTR(STA_deref_Ab_Aa).setAb(addrBaseReg).setAa(srcDataReg)();
  } else {
    INSTR(STA_deref_Ab_off16sx_Aa).setAb(addrBaseReg).setOff16sx(offset16).setAa(srcDataReg)();
  }
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
  return MachineType::INVALID;
}

uint32_t Assembler::getOperandMovCost(ArgType const argType, VariableStorage const &storage) const VB_THROW {
  if (argType == ArgType::NONE) {
    return 0U;
  }

  assert(MachineTypeUtil::getSize(storage.machineType) == 4U && "Only support I32 instruction.");
  if (storage.type == StorageType::INVALID) {
    return invalidMovCost;
  } else if ((storage.type == StorageType::CONSTANT)) {
    uint32_t const value{storage.location.constUnion.u32};
    constexpr size_t operandMatrixLength{(static_cast<size_t>(ArgType::const16sx_32) - static_cast<size_t>(ArgType::d15)) + 1U};
    // [d15, dataReg32_a, dataReg32_b, dataReg32_c, const4sx_32, const8zx_32, const9sx_32, const9zx_32, const16sx_32]
    // Each item in the costs array specifies the length(in bytes) of the instruction required to move storage to argType.
    // For example, the first entry '8U' means mov storage(constant which value larger than 16sx) to d15 need 8 bytes instruction:
    // mov  d15, lower16sx(storage); addih  d15, d15, higher16sx(storage)
    std::array<uint32_t, operandMatrixLength> costs{{8U, 8U, 8U, 8U, invalidMovCost, invalidMovCost, invalidMovCost, invalidMovCost, invalidMovCost}};
    if (SignedInRangeCheck<4U>::check(bit_cast<int32_t>(value)).inRange()) {
      // -8 ~ 7
      if (bit_cast<int32_t>(value) < 0) {
        costs = {2U, 2U, 2U, 2U, 0U, invalidMovCost, 0U, invalidMovCost, 0U};
      } else {
        costs = {2U, 2U, 2U, 2U, 0U, 0U, 0U, 0U, 0U};
      }
    } else if (UnsignedInRangeCheck<8U>::check(value).inRange()) {
      // 0 ~ 255 => [8, 255]
      costs = {2U, 4U, 4U, 4U, invalidMovCost, 0U, 0U, 0U, 0U};
    } else if (SignedInRangeCheck<9U>::check(bit_cast<int32_t>(value)).inRange()) {
      // -256 ~ 255 => [-256, -9]
      costs = {4U, 4U, 4U, 4U, invalidMovCost, invalidMovCost, 0U, invalidMovCost, 0U};
    } else if (UnsignedInRangeCheck<9U>::check(value).inRange()) {
      // 0 ~ 511 => [256, 511]
      costs = {4U, 4U, 4U, 4U, invalidMovCost, invalidMovCost, invalidMovCost, 0U, 0U};
    } else if (SignedInRangeCheck<16U>::check(bit_cast<int32_t>(value)).inRange()) {
      // -32768 ~ 32767 => [-32768, -257] & [512, 32767]
      costs = {4U, 4U, 4U, 4U, invalidMovCost, invalidMovCost, invalidMovCost, invalidMovCost, 0U};
    } else if (UnsignedInRangeCheck<16U>::check(value).inRange()) {
      // 0 ~ 65535 => [32768, 65535]
      costs = {4U, 4U, 4U, 4U, invalidMovCost, invalidMovCost, invalidMovCost, invalidMovCost, invalidMovCost};
    } else {
      static_cast<void>(0);
    }
    return costs[static_cast<uint32_t>(argType) - static_cast<uint32_t>(ArgType::d15)];
  } else if (storage.type == StorageType::REGISTER) {
    REG const reg{storage.location.reg};
    if ((reg == REG::D15) && ((argType == ArgType::d15) || isDataReg32(argType))) {
      return 0U;
    }
    if (RegUtil::isDATA(reg) && isDataReg32(argType)) {
      return 0U;
    }
  } else {
    static_cast<void>(0);
  }

  return invalidMovCost;
}

Assembler::OperandMovement Assembler::getInstructionCost(AbstrInstr const &instruction, VariableStorage const &arg0, VariableStorage const &arg1,
                                                         std::array<bool const, 3> const startedAsWritableScratchReg,
                                                         VariableStorage const &verifiedTargetHint, bool const isD15Available) const VB_THROW {
  constexpr OperandMovement const invalid{invalidMovCost, 0U, false, false, false};
  if (instruction.useD15 && !isD15Available) {
    return invalid;
  }

  if (verifiedTargetHint.machineType != MachineType::INVALID) {
    assert((verifiedTargetHint.type == StorageType::REGISTER) && "Invalid target hint");
    if ((instruction.destType == ArgType::d15) && (verifiedTargetHint.location.reg != REG::D15)) {
      return invalid;
    }
  }

  bool const isArgsSame{arg0.inSameLocation(arg1)};
  bool const isSrcTypeSame{(instruction.src0Type == instruction.src1Type) ||
                           (isDataReg32(instruction.src0Type) && isDataReg32(instruction.src1Type))};
  uint32_t const instructionSize{is16BitInstr(instruction.opcode) ? 2_U32 : 4_U32};

  // coverity[autosar_cpp14_a8_5_2_violation]
  auto const getOperandMovement = [this, isArgsSame, isSrcTypeSame, arg0, arg1, instructionSize](ArgType const src0Type,
                                                                                                 ArgType const src1Type) -> OperandMovement {
    uint32_t const arg0MoveCost{getOperandMovCost(src0Type, arg0)};
    uint32_t const arg1MoveCost{(isArgsSame && isSrcTypeSame) ? 0U : getOperandMovCost(src1Type, arg1)};
    uint32_t const operandMoveCost{
        ((arg0MoveCost == invalidMovCost) || (arg1MoveCost == invalidMovCost)) ? invalidMovCost : (arg0MoveCost + arg1MoveCost + instructionSize)};
    bool const needMoveArg0{needMoveOperand(src0Type, arg0)};
    bool const needMoveArg1{needMoveOperand(src1Type, arg1)};
    uint32_t liftCount{needMoveArg0 ? 1_U32 : 0_U32};
    liftCount += ((arg1MoveCost != 0U) && (arg1MoveCost != invalidMovCost)) ? 1_U32 : 0_U32;
    return {operandMoveCost, liftCount, needMoveArg0, needMoveArg1, false};
  };

  // coverity[autosar_cpp14_a4_5_1_violation]
  OperandMovement const result{getOperandMovement(instruction.src0Type, instruction.src1Type)};
  bool canSrc0DstBeSame{false};

  if (verifiedTargetHint.machineType != MachineType::INVALID) {
    // given targetHint, try to use dest as arg0:
    // 1. if already equals => true
    // 2. arg0 is constant(need to lift to reg) and dest is a writableScratchReg. Then arg0 should lift to targetHint in future
    canSrc0DstBeSame = (verifiedTargetHint.equals(arg0) || (result.movArg0 && startedAsWritableScratchReg[2]));
  } else {
    // no targetHint, try to use arg0 as dest:
    // 1. arg0 is already writable => true, use arg0 reg as dest
    // 1. arg0 is constant, (need to lift to reg) => arg0 will be lifted to scratchReg, use arg0 as dest
    canSrc0DstBeSame = (startedAsWritableScratchReg[0] || result.movArg0);
  }

  if (instruction.src_0_1_commutative) {
    // coverity[autosar_cpp14_a4_5_1_violation]
    OperandMovement swappedResult{getOperandMovement(instruction.src1Type, instruction.src0Type)};
    swappedResult.reversed = true;

    bool canSwappedSrc0DstBeSame{false};
    if (verifiedTargetHint.machineType != MachineType::INVALID) {
      canSwappedSrc0DstBeSame = (verifiedTargetHint.equals(arg1) || (swappedResult.movArg1 && startedAsWritableScratchReg[2]));
    } else {
      canSwappedSrc0DstBeSame = (startedAsWritableScratchReg[1] || swappedResult.movArg1);
    }

    if (instruction.src0_dst_same) {
      if (!canSrc0DstBeSame && !canSwappedSrc0DstBeSame) {
        return invalid;
      } else if (!canSrc0DstBeSame) {
        return swappedResult;
      } else if (!canSwappedSrc0DstBeSame) {
        return result;
      } else {
        static_cast<void>(0);
      }
    }
    return (swappedResult.cost < result.cost) ? swappedResult : result;
  }

  if (instruction.src0_dst_same && !canSrc0DstBeSame) {
    return invalid;
  } else {
    return result;
  }
}

void Assembler::emitAbstrInstr(AbstrInstr const &abstrInstr, VariableStorage const &dest, VariableStorage const &src0, VariableStorage const &src1) {
  assert(elementFitsArgType(abstrInstr.destType, dest) && "Argument doesn't fit instruction");
  assert(elementFitsArgType(abstrInstr.src0Type, src0) && "Argument doesn't fit instruction");
  assert(elementFitsArgType(abstrInstr.src1Type, src1) && "Argument doesn't fit instruction");
  Instruction instruction{INSTR(abstrInstr)};

  // coverity[autosar_cpp14_a8_5_2_violation]
  auto const setOperand = [&instruction](VariableStorage const &storage, ArgType const argType) VB_NOEXCEPT -> void {
    switch (argType) {
    case ArgType::NONE:
      return;
    case ArgType::d15:
      // Since D15 is an implicit register here, do not need encoding.
      break;
    case ArgType::const4sx_32:
      static_cast<void>(instruction.setConst4sx(SafeInt<4U>::fromUnsafe(bit_cast<int32_t>(storage.location.constUnion.u32))));
      break;
    case ArgType::const8zx_32:
      static_cast<void>(instruction.setConst8zx(SafeUInt<8U>::fromUnsafe(storage.location.constUnion.u32)));
      break;
    case ArgType::const9zx_32:
      static_cast<void>(instruction.setConst9zx(SafeUInt<9U>::fromUnsafe(storage.location.constUnion.u32)));
      break;
    case ArgType::const9sx_32:
      static_cast<void>(instruction.setConst9sx(SafeInt<9U>::fromUnsafe(bit_cast<int32_t>(storage.location.constUnion.u32))));
      break;
    case ArgType::const16sx_32:
      static_cast<void>(instruction.setConst16sx(SafeInt<16U>::fromUnsafe(bit_cast<int32_t>(storage.location.constUnion.u32))));
      break;
    case ArgType::dataReg32_a:
      static_cast<void>(instruction.setDa(storage.location.reg));
      break;
    case ArgType::dataReg32_b:
      static_cast<void>(instruction.setDb(storage.location.reg));
      break;
    case ArgType::dataReg32_c:
      static_cast<void>(instruction.setDc(storage.location.reg));
      break;
    default:
      assert(false && "Others ArgType not implement yet");
      break;
    }
  };
  // coverity[autosar_cpp14_a4_5_1_violation]
  setOperand(dest, abstrInstr.destType);
  if (!abstrInstr.src0_dst_same) {
    // coverity[autosar_cpp14_a4_5_1_violation]
    setOperand(src0, abstrInstr.src0Type);
  }
  // coverity[autosar_cpp14_a4_5_1_violation]
  setOperand(src1, abstrInstr.src1Type);

  instruction();
}

} // namespace tc
} // namespace vb
#endif

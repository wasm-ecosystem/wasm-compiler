///
/// @file x86_64_assembler.cpp
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
#include <array>
#include <cassert>
#include <cstdint>

#include "x86_64_assembler.hpp"
#include "x86_64_backend.hpp"
#include "x86_64_cc.hpp"
#include "x86_64_encoding.hpp"

#include "src/core/common/Span.hpp"
#include "src/core/common/TrapCode.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/basedataoffsets.hpp"
#include "src/core/common/implementationlimits.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/Compiler.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_instruction.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_relpatchobj.hpp"
#include "src/core/compiler/common/Common.hpp"
#include "src/core/compiler/common/MachineType.hpp"
#include "src/core/compiler/common/MemWriter.hpp"
#include "src/core/compiler/common/ModuleInfo.hpp"
#include "src/core/compiler/common/RegMask.hpp"
#include "src/core/compiler/common/StackElement.hpp"
#include "src/core/compiler/common/StackType.hpp"
#include "src/core/compiler/common/VariableStorage.hpp"
#include "src/core/compiler/common/util.hpp"

namespace vb {
namespace x86_64 {
using Assembler = x86_64Assembler; ///< Shortcut for x86_64Assembler

Assembler::x86_64Assembler(x86_64_Backend &backend, MemWriter &binary, ModuleInfo &moduleInfo) VB_NOEXCEPT : backend_(backend),
                                                                                                             binary_(binary),
                                                                                                             moduleInfo_(moduleInfo) {
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
  return Instruction(abstrInstr, binary_);
}

// Move a 64-bit immediate to a 64-bit general purpose register, does not check
// whether a 32bit sign-extended move would be more efficient
void Assembler::MOVimm64(REG const reg, uint64_t const imm) const {
  assert(RegUtil::isGPR(reg) && "Only GPR registers allowed");
  INSTR(MOV_r64_imm64_t).setR(reg).setImm64(imm)();
}

// Generate machine code for a WebAssembly TRAP with the given trapCode as hint
void Assembler::TRAP(TrapCode const trapCode, bool const loadTrapCode) const {
  if (backend_.compiler_.getDebugMode()) {
    INSTR(MOV_r32_imm32).setR(WasmABI::REGS::trapPosReg).setImm32(moduleInfo_.bytecodePosOfLastParsedInstruction)();
  }
  if (loadTrapCode) {
    INSTR(MOV_r32_imm32).setR(WasmABI::REGS::trapReg).setImm32(static_cast<uint32_t>(trapCode))();
  }

  prepareJMP(false, CC::NONE).linkToBinaryPos(moduleInfo_.helperFunctionBinaryPositions.genericTrapHandler);
}

void Assembler::cTRAP(TrapCode const trapCode, CC const conditionCode, bool const loadTrapCode) const {
  RelPatchObj const relPatchObj{prepareJMP(true, negateCC(conditionCode))};
  TRAP(trapCode, loadTrapCode);
  relPatchObj.linkToHere();
}

uint32_t Assembler::alignStackFrameSize(uint32_t const frameSize) const VB_NOEXCEPT {
  // Align to 16B (without params)
  return roundUpToPow2(frameSize - moduleInfo_.fnc.paramWidth, 4U) + moduleInfo_.fnc.paramWidth;
}

// Set stack pointer so it points to position after return address and locals
// on stack + frameSize SP <- LOCALS <- RET <- PARAMS
void Assembler::setStackFrameSize(uint32_t const frameSize, bool const temporary, bool const mayRemoveLocals) {
  assert((frameSize == moduleInfo_.getStackFrameSizeBeforeReturn()) || (frameSize == alignStackFrameSize(frameSize)));
  assert(frameSize >= moduleInfo_.getStackFrameSizeBeforeReturn() && "Cannot remove return address and parameters");

  if (!mayRemoveLocals) {
    assert(frameSize >= moduleInfo_.getFixedStackFrameWidth() && "Cannot implicitly drop active variables (tempstack, local) by truncating stack");
  }

  if (moduleInfo_.fnc.stackFrameSize != frameSize) {
    static_assert(ImplementationLimits::maxStackFrameSize < static_cast<uint32_t>(INT32_MAX), "Maximum stack frame size too large");
    if (frameSize > ImplementationLimits::maxStackFrameSize) {
      throw ImplementationLimitationException(ErrorCode::Reached_maximum_stack_frame_size);
    } // GCOVR_EXCL_LINE

    // Always in range due to the check above
    int32_t const delta{static_cast<int32_t>(moduleInfo_.fnc.stackFrameSize) - static_cast<int32_t>(frameSize)};

    // Not allowed to change the flags here, considered cmp -> change sp -> use cmp result
    // LEA will not affect the flags
    INSTR(LEA_r64_m_t).setR(REG::SP).setM4RM(REG::SP, delta)();

    if (!temporary) {
      moduleInfo_.fnc.stackFrameSize = frameSize;
    }

#if ENABLE_EXTENSIONS
    if (backend_.compiler_.getAnalytics() != nullptr) {
      backend_.compiler_.getAnalytics()->updateMaxStackFrameSize(frameSize);
    }
#endif
  }
}

#if ACTIVE_STACK_OVERFLOW_CHECK
void Assembler::checkStackFence() const {
  INSTR(CMP_r64_rm64).setR(REG::SP).setM4RM(WasmABI::REGS::linMem, -Basedata::FromEnd::stackFence)();
  RelPatchObj const inRange = prepareJMP(true, CC::AE);
  TRAP(TrapCode::STACKFENCEBREACHED);
  inRange.linkToHere();
}
#endif

void Assembler::probeStack(uint32_t const delta, REG const scratchReg1, REG const scratchReg2) const {
  assert(scratchReg1 != REG::NONE && scratchReg2 != REG::NONE && "Scratch register needed");

  constexpr uint32_t osPageSize{1_U32 << 12_U32};
  if (delta < osPageSize) {
    return;
  }

  // Move SP to scratchReg1
  INSTR(MOV_r64_rm64).setR(scratchReg1).setR4RM(REG::SP)();
  INSTR(MOV_rm64_imm32sx).setR4RM(scratchReg2).setImm32(delta)();

#ifdef VB_WIN32
  uint32_t const branchTargetOffset = binary_.size();
  INSTR(SUB_rm64_imm32sx).setR4RM(scratchReg1).setImm32(osPageSize)();

  // Probe the position and discard the result
  INSTR(TEST_rm64_r64_t).setM4RM(scratchReg1, 0).setR(scratchReg1)();

  INSTR(SUB_rm64_imm32sx).setR4RM(scratchReg2).setImm32(osPageSize)();
  prepareJMP(true, CC::A).linkToBinaryPos(branchTargetOffset);
#else
  uint32_t const branchTargetOffset{binary_.size()};
  INSTR(SUB_rm64_imm32sx).setR4RM(REG::SP).setImm32(osPageSize)();

  // Probe the position and discard the result
  INSTR(TEST_rm64_r64_t).setM4RM(REG::SP, 0).setR(REG::SP)();

  INSTR(SUB_rm64_imm32sx).setR4RM(scratchReg2).setImm32(osPageSize)();
  prepareJMP(true, CC::A).linkToBinaryPos(branchTargetOffset);

  // Restore the stack pointer
  INSTR(MOV_r64_rm64).setR(REG::SP).setR4RM(scratchReg1)();
#endif
}

/// @details For a given "source" and "destination" StackElement and a list of
/// potentially usable instructions in the form of AbstrInstr, choose the first
/// instruction that matches. That means that "cheaper" instructions, i.e. those
/// using immediate values, should be ordered before more expensive ones. If
/// none of the instructions matches the given arguments, the arguments are, one
/// by one, lifted (loaded into registers). The caller must ensure that at least
/// one instruction is able to match both arguments (either directly after
/// calling or by lifting one or both arguments). As soon as one of the
/// instructions matches both arguments, machine code is produced and the
/// resulting StackElement is returned, including a flag whether the arguments
/// were reversed. (Important for "commutative" comparisons, where the condition
/// then has to be reversed) An optional targetHint specifies where the target
/// can be written, this function does not guarantee that it will actually write
/// the result to that abstract storage location. If a targetHint is given, it
/// is automatically assumed to be writable, irrespective of whether it actually
/// is. If READONLYPtr (inverted NULL pointer) is passed as a targetHint, it is
/// assumed that this instruction writes to neither of the arguments and is thus
/// a readonly instruction like cmp. protRegs specifies a mask of registers
/// that must not be used for lifting
Assembler::ActionResult Assembler::selectInstr(Span<AbstrInstr const> const &instructions, StackElement const *const arg0,
                                               StackElement const *const arg1, StackElement const *const targetHint, RegMask const protRegs,
                                               bool const actionIsReadonly) {
  assert(instructions.size() > 0 && "Zero instructions to select from");

  // Save first commutation and machineType to be able to compare the others in
  // the given array of AbstrInstrs. They have to structurally match.
  bool const commutative{instructions[0].commutative};
  bool const unop{instructions[0].unop};
  MachineType const dstType{machineTypeForArgType(instructions[0].dstType)};
  MachineType const srcType{machineTypeForArgType(instructions[0].srcType)};

  assert(((dstType != MachineType::INVALID) || (srcType != MachineType::INVALID)) &&
         "Two invalid MachineTypes are not allowed for instruction selection");
  assert((unop || ((dstType != MachineType::INVALID) && (srcType != MachineType::INVALID))) &&
         "Non-unary AbstrInstr must not have any invalid ArgTypes");
  assert((!unop || !commutative) && "Unary operation cannot be commutative");
  assert(((dstType != MachineType::INVALID) || !arg0) && (srcType != MachineType::INVALID || !arg1) &&
         "Invalid instruction argument mandates NULL as input");
  assert(((srcType == MachineType::INVALID) || arg1) && "Source argument missing, even though instruction mandates one");
  assert((unop || (dstType == MachineType::INVALID) || arg0) && "Dest argument missing, even though instruction mandates one");

  StackElement const *const verifiedTargetHint{
      ((dstType != MachineType::INVALID) && (backend_.getUnderlyingRegIfSuitable(targetHint, dstType, protRegs) != REG::NONE)) ? targetHint
                                                                                                                               : nullptr};

  // Check whether the args could theoretically be used to write the result to,
  // this is true if they are scratch registers or temporary stack memory
  // storage locations and are not on the stack, except for If one of the args
  // is NULL or it's neither a scratch register or a temporary storage location
  // on the stack, we must not overwrite it (except if it equals the targetHint)
  // See whether we can find an element with the same storage location but with
  // a different pointer, if yes the argument cannot be directly used as a
  // destination since we must not overwrite it
  std::array<const bool, 2> const startedAsWritableScratchReg{{backend_.isWritableScratchReg(arg0), backend_.isWritableScratchReg(arg1)}};
  std::array<bool, 2> argCanBeDst{};
  if (!actionIsReadonly) {
    argCanBeDst[0] = startedAsWritableScratchReg[0] || backend_.common_.inSameReg(arg0, verifiedTargetHint, true);
    argCanBeDst[1] = startedAsWritableScratchReg[1] || backend_.common_.inSameReg(arg1, verifiedTargetHint, true);
  } else {
    argCanBeDst[0] = false;
    argCanBeDst[1] = false;
  }

  constexpr StackElement invalidElem{StackElement::invalid()};
  std::array<StackElement, 2> inputArgs{{(arg0 != nullptr) ? *arg0 : invalidElem, (arg1 != nullptr) ? *arg1 : invalidElem}};

  // Check whether both are equal to another and not INVALID
  bool argsAreEqual{StackElement::equalsVariable(&inputArgs[0], &inputArgs[1])};

  // Lambda functions that can be used to lift the arguments
  std::array<bool, 2> argHasBeenLifted{{false, false}};
  bool changed{false};
  // coverity[autosar_cpp14_a8_5_2_violation]
  auto liftArgLambda = [this, &argHasBeenLifted, &inputArgs, &argCanBeDst, &changed, argsAreEqual, commutative, protRegs,
                        verifiedTargetHint](uint32_t const idx, bool const coLift = false) mutable {
    assert(!argHasBeenLifted[idx] && "Cannot lift arg twice");
    assert((!protRegs.allMarked()) && "Cannot lift");
    assert(idx <= 1U && "Lift index out of range"); // As we only have two args, idx must be 0 or 1

    // otherIdx is 1 if idx is 0, else otherIdx is 0
    uint32_t const otherIdx{idx ^ 1U};
    if (argsAreEqual && argHasBeenLifted[otherIdx]) {
      inputArgs[idx] = inputArgs[otherIdx];
    } else {
      RegAllocTracker tempRegAllocTracker{};
      tempRegAllocTracker.writeProtRegs = protRegs | backend_.mask(&inputArgs[otherIdx]);
      static_cast<void>(backend_.common_.liftToRegInPlaceProt(inputArgs[idx], true, ((idx == 1U) && (!commutative)) ? nullptr : verifiedTargetHint,
                                                              tempRegAllocTracker));
    }
    // Lifted arg can now be dest, as it's now guaranteed to be in a writable register
    argCanBeDst[idx] = true;
    argHasBeenLifted[idx] = true;
    changed = true;

    // If both args are equal, set the other arg to the newly lifted one and
    // also set argCanBeDst accordingly
    if ((coLift && argsAreEqual) && (!argHasBeenLifted[otherIdx])) {
      inputArgs[otherIdx] = inputArgs[idx];
      argCanBeDst[otherIdx] = true;
      argHasBeenLifted[otherIdx] = true;
    }
  };

  if (unop) {
    if (((!actionIsReadonly) && (srcType == MachineType::INVALID)) && (!argCanBeDst[0])) {
      // Destination needs to be writable, lift to writable register if not
      // already in one
      liftArgLambda(0U);
    } else if ((dstType != MachineType::INVALID) && (srcType != MachineType::INVALID)) {
      // Unary operation with a source AND destination, we need to choose a
      // destination for that case since we only have a single argument and an
      // optional targetHint
      if (verifiedTargetHint != nullptr) {
        inputArgs[0] = *verifiedTargetHint;
      } else if (argCanBeDst[1]) {
        // Determine whether the source and destination type are such that they
        // can be stored in the same type of register, i.e. I32 can be stored in
        // the same GPR as I64, analogously for floats
        bool const argTypesAreCompatible{MachineTypeUtil::isInt(dstType) == MachineTypeUtil::isInt(srcType)};
        if (argTypesAreCompatible) {
          StackElement newInputArg{inputArgs[1]};
          uint32_t const typeInt{(static_cast<uint32_t>(newInputArg.type) & static_cast<uint32_t>(StackType::BASEMASK)) |
                                 static_cast<uint32_t>(MachineTypeUtil::toStackTypeFlag(dstType))};
          newInputArg.type = static_cast<StackType>(typeInt);
          inputArgs[0] = newInputArg;
          argsAreEqual = true;
        } else {
          if (arg0 == nullptr) {
            RegAllocTracker regAllocTracker{};
            regAllocTracker.writeProtRegs = protRegs;
            inputArgs[0] = backend_.common_.reqScratchRegProt(dstType, regAllocTracker, false).elem;
          }
        }
      } else {
        RegAllocTracker regAllocTracker{};
        regAllocTracker.writeProtRegs = protRegs;
        inputArgs[0] = backend_.common_.reqScratchRegProt(dstType, regAllocTracker, false).elem;
      }
      argCanBeDst[0] = true;
    } else {
      static_cast<void>(0);
    }
  } else {
    if (!actionIsReadonly) {
      bool const arg0IsFloatConstant{(inputArgs[0].type == StackType::CONSTANT_F32) || (inputArgs[0].type == StackType::CONSTANT_F64)};
      bool const arg1IsFloatConstant{(inputArgs[1].type == StackType::CONSTANT_F32) || (inputArgs[1].type == StackType::CONSTANT_F64)};

      // Always lift floating point constants, because they cannot be used as immediates in x86 anyway
      // Also lift first argument if it cannot be used as destination and the operation is not commutative
      // Do not co-lift if both are constants
      if (arg0IsFloatConstant || ((!commutative) && (!argCanBeDst[0]))) {
        bool const arg1IsConstant{inputArgs[1].getBaseType() == StackType::CONSTANT};
        liftArgLambda(0U, !arg1IsConstant);
      }
      if (arg1IsFloatConstant) {
        liftArgLambda(1U);
      }
    }
  }

  std::array<bool, 2> argHasMatched{{false, false}};
  // 3 tries because we might check (1), lift the source, check again (2) then
  // lift the destination and then check again (3). Third try is the latest
  // where it is guaranteed that both source and destination are in registers
  for (uint32_t tries{0U}; tries < 3U; tries++) {
    for (uint32_t instrIdx{0U}; instrIdx < static_cast<uint32_t>(instructions.size()); instrIdx++) {
      // Current instruction from the array of given instructions which will be
      // checked for matching arguments
      AbstrInstr const actionArg{instructions[instrIdx]};

      assert(actionArg.commutative == commutative && actionArg.unop == unop && "Choosable instructions must be uniformly commutative");
      assert(dstType == machineTypeForArgType(actionArg.dstType) && srcType == machineTypeForArgType(actionArg.srcType) &&
             "Choosable instructions must have uniformly typed ArgTypes");

      // If it's no unary operation, it is commutative and the args are not
      // equal, we have two commutation tries (one for each order), otherwise we
      // only check the given order
      bool const checkReversedOrder{commutative && (!argsAreEqual)};
      uint32_t const commutationTries{checkReversedOrder ? 2_U32 : 1_U32};
      std::array<VariableStorage, 2U> const inputStorages{{
          moduleInfo_.getStorage(inputArgs[0U]),
          moduleInfo_.getStorage(inputArgs[1U]),
      }};
      bool argsMatched{false};
      uint32_t matchedArgIndexToUseAsDst{UINT32_MAX};

      for (uint32_t argIndexToUseAsDst{0U}; argIndexToUseAsDst < commutationTries; argIndexToUseAsDst++) {
        uint32_t const argIndexToUseAsSrc{argIndexToUseAsDst ^ 1U};

        std::array<bool, 2> argMatches{{false, false}};
        if ((actionIsReadonly || argCanBeDst[argIndexToUseAsDst]) || (dstType == MachineType::INVALID)) {
          argMatches[argIndexToUseAsDst] = elementFitsArgType(actionArg.dstType, inputStorages[argIndexToUseAsDst]);
        }
        argMatches[argIndexToUseAsSrc] = elementFitsArgType(actionArg.srcType, inputStorages[argIndexToUseAsSrc]);

        argHasMatched[argIndexToUseAsDst] = argMatches[argIndexToUseAsDst] || argHasMatched[argIndexToUseAsDst];
        argHasMatched[argIndexToUseAsSrc] = argMatches[argIndexToUseAsSrc] || argHasMatched[argIndexToUseAsSrc];

        if (argsAreEqual) {
          argHasMatched[argIndexToUseAsDst] = argMatches[argIndexToUseAsSrc] || argHasMatched[argIndexToUseAsDst];
          argHasMatched[argIndexToUseAsSrc] = argMatches[argIndexToUseAsDst] || argHasMatched[argIndexToUseAsSrc];
        }

        if (argMatches[0] && argMatches[1]) {
          if (argsMatched) {
            // both args matched, we need to decide which one to use as dst
            if ((verifiedTargetHint != nullptr) &&
                inputStorages[matchedArgIndexToUseAsDst ^ 1U].inSameLocation(backend_.moduleInfo_.getStorage(*verifiedTargetHint))) {
              // use current one as dst because it is same as targetHint
              matchedArgIndexToUseAsDst = argIndexToUseAsDst;
            }
          } else {
            argsMatched = true;
            matchedArgIndexToUseAsDst = argIndexToUseAsDst;
          }
        }
      }
      if (argsMatched) {
        emitActionArg(actionArg, inputStorages[matchedArgIndexToUseAsDst], inputStorages[matchedArgIndexToUseAsDst ^ 1U]);
        // coverity[autosar_cpp14_a16_2_3_violation]
        Assembler::ActionResult actionResult{};
        actionResult.reversed = matchedArgIndexToUseAsDst != 0U;
        actionResult.element =
            (dstType == MachineType::INVALID) ? invalidElem : backend_.common_.getResultStackElement(&inputArgs[matchedArgIndexToUseAsDst], dstType);
        return actionResult;
      }
    }

    // Either lifting into registers is protected by protRegs and the first
    // instruction didn't match or we have already lifted both and there is
    // still no instruction that matches the arguments This should never happen
    // if validation is done before calling this function
    assert((!protRegs.allMarked()) && tries < 2 && "Instruction selection error");

    // lift and try again
    if (unop) {
      if (dstType == MachineType::INVALID) {
        // lift arg2 to (non-necessarily) writable reg, could also be targetHint
        // to not waste another register for an unop; but as we are actively
        // lifting anyway, it's writable either way
        liftArgLambda(1U);
      } else if (srcType == MachineType::INVALID) {
        liftArgLambda(0U);
      } else {
        changed = false;
        if (!argHasMatched[0]) {
          liftArgLambda(0U);
        }
        // always source
        if ((!(changed && argsAreEqual)) && (!argHasMatched[1])) {
          liftArgLambda(1U);
        }
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
      // If at least one of the arguments was lifted, we continue
      if (changed) {
        continue;
      }

      if ((!argHasBeenLifted[0]) && (inputArgs[1].getBaseType() == StackType::CONSTANT)) {
        liftArgLambda(0U);
      } else if ((!argHasBeenLifted[1]) && (inputArgs[0].getBaseType() == StackType::CONSTANT)) {
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

// Returns the underlying MachineType for a given ArgType
MachineType Assembler::machineTypeForArgType(ArgType const argType) VB_NOEXCEPT {
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

bool Assembler::elementFitsArgType(ArgType const argType, VariableStorage const &storage) const VB_NOEXCEPT {
  // NONE matches everything, even NULL
  if (argType == ArgType::NONE) {
    return true;
  }
  // INVALID only matches with ArgType::NONE
  if (storage.type == StorageType::INVALID) {
    return false;
  }

  // element is guaranteed not to be NULL
  StorageType const elementStorageType{storage.type};
  MachineType const machineType{storage.machineType};
  if (elementStorageType == StorageType::INVALID) {
    return false;
  } else if (elementStorageType == StorageType::CONSTANT) {
    if (machineType == MachineType::I32) {
      if ((argType == ArgType::imm32) || (argType == ArgType::imm8_32)) {
        return true;
      }
      if ((argType == ArgType::c1_32) && (storage.location.constUnion.u32 == 1U)) {
        return true;
      }
      if ((argType == ArgType::imm8sx_32) &&
          ((bit_cast<int32_t>(storage.location.constUnion.u32) >= INT8_MIN) && (bit_cast<int32_t>(storage.location.constUnion.u32) <= INT8_MAX))) {
        return true;
      }
    } else if (machineType == MachineType::I64) {
      if (argType == ArgType::imm8_64) {
        return true;
      }
      if ((argType == ArgType::c1_64) && (storage.location.constUnion.u64 == 1U)) {
        return true;
      }
      if ((argType == ArgType::imm8sx_64) && in_range<int8_t>(bit_cast<int64_t>(storage.location.constUnion.u64))) {
        return true;
      }
      if ((argType == ArgType::imm32sx_64) && in_range<int32_t>(bit_cast<int64_t>(storage.location.constUnion.u64))) {
        return true;
      }
    } else {
      static_cast<void>(0);
    }
  } else if (elementStorageType == StorageType::REGISTER) {
    if (MachineTypeUtil::isInt(machineType) && ((argType == ArgType::r32) || (argType == ArgType::rm32))) {
      return true;
    }
    if (MachineTypeUtil::isInt(machineType) && ((argType == ArgType::r64) || (argType == ArgType::rm64))) {
      return true;
    }
    if ((machineType == MachineType::F32) &&
        (((argType == ArgType::r32f) || (argType == ArgType::rm32f)) || (argType == ArgType::rm32f_128_restrictm))) {
      return true;
    }
    if ((machineType == MachineType::F64) &&
        (((argType == ArgType::r64f) || (argType == ArgType::rm64f)) || (argType == ArgType::rm64f_128_restrictm))) {
      return true;
    }
  } else { // Memory
    if ((machineType == MachineType::I32) && (argType == ArgType::rm32)) {
      return true;
    }
    if ((machineType == MachineType::I64) && (argType == ArgType::rm64)) {
      return true;
    }
    if ((machineType == MachineType::F32) && (argType == ArgType::rm32f)) {
      return true;
    }
    if ((machineType == MachineType::F64) && (argType == ArgType::rm64f)) {
      return true;
    }
  }
  return false;
}

// Given an opcode, a destination and an (optional) source StackElement (Can be
// register/memory/constant abstracted by locals, globals or direct ones),
// assemble the given instruction.
void Assembler::emitActionArg(AbstrInstr const &actionArg, VariableStorage const &arg0, VariableStorage const &arg1) {
  // Should never happen, because it should have been selected appropriately before
  assert(elementFitsArgType(actionArg.dstType, arg0) && elementFitsArgType(actionArg.srcType, arg1) && "Arguments don't fit instruction");

  Instruction instruction{INSTR(actionArg)};
  // coverity[autosar_cpp14_a8_5_2_violation]
  auto const setInstructionOperand = [this, &actionArg, &instruction](VariableStorage const &arg, ArgType const argType) -> void {
    if (arg.type == StorageType::INVALID) {
      // Guaranteed not to be INVALID
      return;
    }
    // If we are expecting ONLY a register
    if (((argType == ArgType::r32) || (argType == ArgType::r64)) || ((argType == ArgType::r32f) || (argType == ArgType::r64f))) {
      // If it is used as register opcode extension, i.e. regular register argument
      if (actionArg.opTemplate.extension == OPCodeExt::R) {
        static_cast<void>(instruction.setR(arg.location.reg));
      } else {
        // If there is a register extension, set it for RM byte (e.g. PSRLD/Q, PSLLD/Q)
        static_cast<void>(instruction.setR4RM(arg.location.reg));
      }
    } else if (((argType == ArgType::imm32) || (argType == ArgType::imm8sx_32)) || (argType == ArgType::imm8_32)) {
      // If we are dealing with a 8-bit or 32-bit immediate representing a
      // 32-bit immediate, sign extended or full
      if (argType == ArgType::imm32) {
        static_cast<void>(instruction.setImm32(arg.location.constUnion.u32));
      } else {
        static_cast<void>(instruction.setImm8(static_cast<uint8_t>(arg.location.constUnion.u32 & 0xFFU)));
      }
    } else if (((argType == ArgType::imm32sx_64) || (argType == ArgType::imm8sx_64)) || (argType == ArgType::imm8_64)) {
      // 8-bit or 32-bit immediate representing a 64-bit immediate
      if (argType == ArgType::imm32sx_64) {
        static_cast<void>(instruction.setImm32(static_cast<uint32_t>(arg.location.constUnion.u64)));
      } else {
        static_cast<void>(instruction.setImm8(static_cast<uint8_t>(arg.location.constUnion.u64)));
      }
    } else if ((((argType == ArgType::rm32) || (argType == ArgType::rm64)) ||
                ((argType == ArgType::rm32f) || (argType == ArgType::rm32f_128_restrictm))) ||
               ((argType == ArgType::rm64f) || (argType == ArgType::rm64f_128_restrictm))) {
      if (arg.type == StorageType::REGISTER) {
        static_cast<void>(instruction.setR4RM(arg.location.reg));
      } else {
        assert(argType != ArgType::rm32f_128_restrictm && argType != ArgType::rm64f_128_restrictm && "Instruction not suitable for memory");
        x86_64_Backend::RegDisp const regDisp{backend_.getMemRegDisp(arg)};
        static_cast<void>(instruction.setM4RM(regDisp.reg, regDisp.disp));
      }
    } else {
      static_cast<void>(0);
    }
  };
  // coverity[autosar_cpp14_a4_5_1_violation]
  setInstructionOperand(arg0, actionArg.dstType);
  // coverity[autosar_cpp14_a4_5_1_violation]
  setInstructionOperand(arg1, actionArg.srcType);
  instruction.emitCode();
}

// Generate machine code for a 8-bit oder 32-bit JMP instruction with optional
// condition code. Bytes to jump from the end of the JMP instruction are given
// as rawOffset. shortJmp is given separately from rawOffset because often
// dummy JMPs are emitted which are patched later on with the correct offset.
RelPatchObj Assembler::prepareJMP(bool const shortJmp, CC const conditionCode) const {
  if (shortJmp) {
    if (conditionCode == CC::NONE) {
      INSTR(JMP_rel8_t).setRel8(0x00)();
    } else {
      INSTR(JCC_rel8_t).setRel8(0x00).setCC(conditionCode)();
    }
  } else {
    if (conditionCode == CC::NONE) {
      INSTR(JMP_rel32_t).setRel32(0x00)();
    } else {
      INSTR(JCC_rel32_t).setRel32(0x00).setCC(conditionCode)();
    }
  }
  return RelPatchObj(shortJmp, binary_.size(), binary_);
}

RelPatchObj Assembler::preparePCRelAddrLEA(REG const targetReg) const {
  INSTR(LEA_r64_m_t).setR(targetReg).setMIP4RM(0)();
  return RelPatchObj(false, binary_.size(), binary_);
}

} // namespace x86_64
} // namespace vb
#endif

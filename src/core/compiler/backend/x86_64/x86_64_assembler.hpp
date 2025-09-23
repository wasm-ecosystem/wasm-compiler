///
/// @file x86_64_assembler.hpp
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
#ifndef X86_64_ASSEMBLER_HPP
#define X86_64_ASSEMBLER_HPP

#include <cstdint>

#include "x86_64_encoding.hpp"
#include "x86_64_instruction.hpp"
#include "x86_64_relpatchobj.hpp"

#include "src/core/common/Span.hpp"
#include "src/core/common/TrapCode.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/common/ModuleInfo.hpp"
#include "src/core/compiler/common/RegMask.hpp"
#include "src/core/compiler/common/Stack.hpp"
#include "src/core/compiler/common/VariableStorage.hpp"

namespace vb {
namespace x86_64 {

class x86_64_Backend;

///
/// @brief x86_64 assembler class
///
class x86_64Assembler final {
public:
  ///
  /// @brief Assembler class that emits machine code, controlled by the backend
  ///
  x86_64Assembler(x86_64_Backend &backend, MemWriter &binary, ModuleInfo &moduleInfo) VB_NOEXCEPT;

  ///
  /// @brief Correctly align the stack frame size
  ///
  /// @param frameSize Stack frame size to align
  /// @return uint32_t Aligned stack frame size
  uint32_t alignStackFrameSize(uint32_t const frameSize) const VB_NOEXCEPT;

  ///
  /// @brief Set the current function's stack frame size
  ///
  /// Includes temporary variables, local variables, return address and parameters in that order)
  ///
  /// @param frameSize New function frame size
  /// @param temporary Whether function frame size adjustment is only performed conditionally (temporary = true; e.g.
  /// wrapped in a conditional branch)
  /// @param mayRemoveLocals Whether this function can remove locals (e.g. right before a return)
  void setStackFrameSize(uint32_t const frameSize, bool const temporary = false, bool const mayRemoveLocals = false);

  ///
  /// @brief Return type of selectInstr that contains a resulting StackElement (representation of variable location) and
  /// whether the instruction switched arguments if it is commutative
  ///
  class ActionResult final {
  public:
    /// @brief Resulting StackElement representing the location where the output of the instruction is placed
    StackElement element;
    /// @brief Whether the instructions input arguments were swapped (only possible if the input sources are set
    /// commutative)
    bool reversed = false;
  };

  ///
  /// @brief Selects an instruction for input StackElements from an array of abstract instructions and writes machine
  /// code to the output binary
  ///
  /// For a given "source" and "destination" StackElement and a list of potentially usable instructions in the form of
  /// AbstrInstr, choose the first instruction that matches the inputs. This means that "cheaper" instructions, i.e.
  /// those using immediate values, should be ordered before more expensive ones. If none of the instructions matches
  /// the given arguments, the arguments are, one by one, lifted (loaded into registers). The caller must ensure that at
  /// least one instruction is able to match both arguments (either directly after calling or by lifting one or both
  /// arguments). As soon as one of the instructions matches both arguments, machine code is produced and the resulting
  /// StackElement is returned, including a flag whether the arguments were reversed. (Important for "commutative"
  /// comparisons, where the condition then has to be reversed) An optional targetHint specifies where the target can be
  /// written, this function does not guarantee that it will actually write the result to that abstract storage
  /// location. If a targetHint is given, it is automatically assumed to be writable, irrespective of whether it
  /// actually is. protRegs specifies a mask of registers that must not be used for lifting
  ///
  /// @param instructions Reference to an array of instructions. Instructions have to be consistent in regard to their
  /// input and output types, arity and commutation
  /// @param arg0 First input for the instruction. The destination register or the destination-source, depends on
  /// instruction. Passing null means auto choose one.
  /// @param arg1 Second input for the instruction (can be nullptr if the instruction only has a single input)
  /// @param targetHint Optional target hint that can be used as a scratch register if it is appropiate (Must be nullptr
  /// for readonly instructions like CMP; can optionally be nullptr in all other cases)
  /// @param protRegs Protected register mask (i.e. which registers not to use)
  /// @param actionIsReadonly Whether this instruction does not write any data to the first operand (e.g. a CMP
  /// instruction)
  /// @return ActionResult Indicating whether the instruction inputs were switched (reversed) and where the result of
  /// the instruction nis now stored
  /// @throws vb::RuntimeError If not enough memory is available
  ActionResult selectInstr(Span<AbstrInstr const> const &instructions, StackElement const *const arg0, StackElement const *const arg1,
                           StackElement const *const targetHint, RegMask const protRegs, bool const actionIsReadonly);

  ///
  /// @brief Wrapper for selectInstr
  ///
  /// @tparam N Size of Array
  /// @param instructions Reference to an array of instructions. Instructions have to be consistent in regard to their
  /// input and output types, arity and commutation
  /// @param arg0 First input for the instruction. The destination register or the destination-source, depends on
  /// instruction. Passing null means auto choose one.
  /// @param arg1 Second input for the instruction (can be nullptr if the instruction only has a single input)
  /// @param targetHint Optional target hint that can be used as a scratch register if it is appropiate (Must be nullptr
  /// for readonly instructions like CMP; can optionally be nullptr in all other cases)
  /// @param protRegs Protected register mask (i.e. which registers not to use)
  /// @param actionIsReadonly Whether this instruction does not write any data to the first operand (e.g. a CMP
  /// instruction)
  /// @return ActionResult Indicating whether the instruction inputs were switched (reversed) and where the result of
  /// the instruction nis now stored
  /// @throws vb::RuntimeError If not enough memory is available
  template <typename AbstrInstrArr>
  inline ActionResult selectInstr(AbstrInstrArr const &instructions, StackElement const *const arg0, StackElement const *const arg1,
                                  StackElement const *const targetHint, RegMask const protRegs, bool const actionIsReadonly) VB_THROW {
    return selectInstr(Span<AbstrInstr const>{instructions.data(), instructions.size()}, arg0, arg1, targetHint, protRegs, actionIsReadonly);
  }

  ///
  /// @brief Emit a JMP instruction with an undefined dummy-offset that must be patched later
  ///
  /// @param shortJmp Whether to emit a jmp with a relative offset of INT8_MAX and INT8_MIN, respectively (Otherwise the
  /// bounds are INT32_MAX and INT32_MIN)
  /// @param conditionCode Condition code for which to branch (CC::NONE for unconditional JMP)
  /// @return RelPatchObj RelPatchObj that will be used to patch the target
  /// @throws std::range_error If not enough memory is available
  RelPatchObj prepareJMP(bool const shortJmp, CC const conditionCode = CC::NONE) const;

  ///
  /// @brief Load a PC-relative address (pointing to a linked RelPatchObj) to the given register using the LEA_r64_m
  /// instruction
  ///
  /// @param targetReg Register to put the absolute target address/pointer into
  /// @return RelPatchObj that should be linked to the target address
  RelPatchObj preparePCRelAddrLEA(REG const targetReg) const;

  ///
  /// @brief Emits instructions that will raise a Wasm trap
  ///
  /// @param trapCode Identifier for the trap reason (will be passed to TrapException)
  /// @param loadTrapCode Whether the TrapCode should be loaded from the given enum or whether it should simply use what
  /// currently is in REGS::trapReg
  /// @throws std::range_error If not enough memory is available
  void TRAP(TrapCode const trapCode, bool const loadTrapCode = true) const;

  ///
  /// @brief Emits instructions that will conditionally raise a Wasm trap based on the conditionCode and the current CPU
  /// status flags
  ///
  /// @param trapCode Identifier for the trap reason (will be passed to TrapException)
  /// @param conditionCode Condition code in which case to trap
  /// @param loadTrapCode Whether the TrapCode should be loaded from the given enum or whether it should simply use what
  /// currently is in REGS::trapReg
  /// @throws std::range_error If not enough memory is available
  void cTRAP(TrapCode const trapCode, CC const conditionCode, bool const loadTrapCode = true) const;

  ///
  /// @brief Move a 64-bit immediate value to a general purpose register
  ///
  /// Moves irrespective of what this register is currently containing, will not spill another register
  ///
  /// @param reg Register to move the immediate to
  /// @param imm 64-bit immediate value to move to the register
  /// @throws std::range_error If not enough memory is available
  void MOVimm64(REG const reg, uint64_t const imm) const;

#if ACTIVE_STACK_OVERFLOW_CHECK
  ///
  /// @brief Check whether the stack pointer is below the stack fence; if so, trap with TrapCode::STACKFENCEBREACHED
  ///
  void checkStackFence() const;
#endif

  ///
  /// @brief Stack probing mechanism similar to Windows' _chkstk() function
  /// This function probes a newly allocated stack portion page by page so auto-extension via guard pages is correctly
  /// triggered
  ///
  /// @param delta By how much the stack pointer will be decreased
  /// @param scratchReg1 Scratch register this routine can use
  /// @param scratchReg2 Scratch register this routine can use
  /// @throws vb::RuntimeError If not enough memory is available
  void probeStack(uint32_t const delta, REG const scratchReg1, REG const scratchReg2) const;

  ///
  /// @brief Generates an instruction instance from an OPCode targeting the binary of the assembler
  ///
  /// @param opcode OPCode template of the instruction
  /// @return Instruction instance that can be emitted
  Instruction INSTR(OPCodeTemplate const opcode) const VB_NOEXCEPT;
  ///
  /// @brief Generates an instruction instance from an AbstrInstr targeting the binary of the assembler
  ///
  /// @param abstrInstr Abstract instruction representation of the instruction
  /// @return Instruction instance that can be emitted
  Instruction INSTR(AbstrInstr const &abstrInstr) const VB_NOEXCEPT;

private:
  x86_64_Backend &backend_; ///< Reference to the backend instance
  MemWriter &binary_;       ///< Reference to the output binary
  ModuleInfo &moduleInfo_;  ///< Reference to the module info struct

  ///
  /// @brief Converts an ArgType to its corresponding MachineType
  ///
  /// @param argType Input ArgType
  /// @return Corresponding MachineType
  static MachineType machineTypeForArgType(ArgType const argType) VB_NOEXCEPT;

  ///
  /// @brief Determines whether an storage matches a given ArgType, including bit size and storage location
  /// @details Check whether a given VariableStorage fits a given ArgType, i.e. whether it can
  /// be used in its current location (register/memory/constant) for a machine
  /// code instruction and whether the MachineType of the element fits.
  ///
  /// e.g. A VariableStorage representing a 32-bit integer will only match a 32-bit integer ArgType and no 64-bit integer
  /// types or floating point ArgTypes
  /// e.g. A VariableStorage in memory will only match ArgType also in memory
  ///
  /// @param argType ArgType to match
  /// @param storage VariableStorage
  /// @return bool Whether the element and ArgType match
  bool elementFitsArgType(ArgType const argType, VariableStorage const &storage) const VB_NOEXCEPT;

  ///
  /// @brief Emits machine code (assembles the instruction) in the corresponding encoding for the given instruction and
  /// source and destination StackElements
  ///
  /// Caller must ensure that the instruction and arguments match
  ///
  /// @param actionArg Abstract representation of an AArch64 instruction
  /// @param arg0 Representation of the first argument of the instruction (destination or first input)
  /// @param arg1 Representation of the second source of the instruction, can be nullptr if the instruction allows
  /// @throws vb::RuntimeError If not enough memory is available
  void emitActionArg(AbstrInstr const &actionArg, VariableStorage const &arg0, VariableStorage const &arg1);
};

} // namespace x86_64
} // namespace vb

#endif /* X86_64_ASSEMBLER_HPP */

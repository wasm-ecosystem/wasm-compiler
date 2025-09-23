///
/// @file aarch64_assembler.hpp
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
#ifndef AARCH64_ASSEMBLER_HPP
#define AARCH64_ASSEMBLER_HPP

#include <cstdint>

#include "aarch64_encoding.hpp"
#include "aarch64_instruction.hpp"
#include "aarch64_relpatchobj.hpp"

#include "src/core/common/FunctionRef.hpp"
#include "src/core/common/Span.hpp"
#include "src/core/common/TrapCode.hpp"
#include "src/core/compiler/common/ModuleInfo.hpp"
#include "src/core/compiler/common/RegMask.hpp"
#include "src/core/compiler/common/VariableStorage.hpp"

namespace vb {
namespace aarch64 {
// coverity[autosar_cpp14_m3_2_3_violation]
class AArch64_Backend;

///
/// @brief Assembler class that emits machine code, controlled by the backend
///
class AArch64_Assembler {
public:
  ///
  /// @brief Construct a new assembler instance for AArch64
  ///
  /// @param backend
  /// @param binary
  /// @param moduleInfo
  AArch64_Assembler(AArch64_Backend &backend, MemWriter &binary, ModuleInfo &moduleInfo) VB_NOEXCEPT;

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
  /// @brief Correctly align the stack frame size
  ///
  /// @param frameSize Stack frame size to align
  /// @return uint32_t Aligned stack frame size
  uint32_t alignStackFrameSize(uint32_t const frameSize) const VB_NOEXCEPT;

#if ACTIVE_STACK_OVERFLOW_CHECK
  ///
  /// @brief Check whether the stack pointer is below the stack fence; if so, trap with TrapCode::STACKFENCEBREACHED
  ///
  /// @param scratchReg Register to use as a scratch register
  void checkStackFence(REG const scratchReg);
#endif

  ///
  /// @brief Stack probing mechanism similar to Windows' _chkstk() function
  /// This function probes a newly allocated stack portion page by page so auto-extension via guard pages is correctly
  /// triggered
  ///
  /// @param delta By how much the stack pointer will be decreased
  /// @param scratchReg1 Scratch register this routine can use
  /// @param scratchReg2 Scratch register this routine can use
  void probeStack(uint32_t const delta, REG const scratchReg1, REG const scratchReg2) const;

  ///
  /// @brief Return type of selectInstr that contains a resulting Storage (representation of variable location) and
  /// whether the instruction switched arguments if it is commutative
  ///
  class ActionResult final {
  public:
    /// @brief Resulting VariableStorage representing the location where the output of the instruction is placed
    VariableStorage storage;
    /// @brief Whether the instructions input arguments were swapped (only possible if the input sources are set
    /// commutative)
    bool reversed = false;
  };

  ///
  /// @brief Selects an instruction for input StackElements from an array of abstract instructions and writes machine
  /// code to the output binary
  ///
  /// For a given "source" and "destination" VariableStorage and a list of potentially usable instructions in the form of
  /// AbstrInstr, choose the first instruction that matches the inputs. This means that "cheaper" instructions, i.e.
  /// those using immediate values, should be ordered before more expensive ones. If none of the instructions matches
  /// the given arguments, the arguments are, one by one, lifted (loaded into registers). The caller must ensure that at
  /// least one instruction is able to match both arguments (either directly after calling or by lifting one or both
  /// arguments). As soon as one of the instructions matches both arguments, machine code is produced and the resulting
  /// VariableStorage is returned, including a flag whether the arguments were reversed. (Important for "commutative"
  /// comparisons, where the condition then has to be reversed) An optional targetHint specifies where the target can be
  /// written, this function does not guarantee that it will actually write the result to that abstract storage
  /// location. If a targetHint is given, it is automatically assumed to be writable, irrespective of whether it
  /// actually is. protRegs specifies a mask of registers that must not be used for lifting
  ///
  /// @param instructions Reference to an array of instructions. Instructions have to be consistent in regard to their
  /// input and output types, arity and commutation
  /// @param inputStorages variable storage of input operands.
  /// @param startedAsWritableScratchReg whether reg in input storage is writable scratch register.
  /// @param targetHint Optional target hint that can be used as a scratch register if it is appropiate (Must be nullptr
  /// for readonly instructions like CMP; can optionally be nullptr in all other cases)
  /// @param protRegs Protected register mask (i.e. which registers not to use)
  /// @param presFlags @see Common::reqScratchRegProt presFlags
  /// @return ActionResult Indicating whether the instruction inputs were switched (reversed) and where the result of
  /// the instruction nis now stored
  ActionResult selectInstr(Span<AbstrInstr const> const &instructions, std::array<VariableStorage, 2U> &inputStorages,
                           std::array<bool const, 2> const startedAsWritableScratchReg, StackElement const *const targetHint, RegMask const protRegs,
                           bool const presFlags) VB_THROW;

  ///
  /// @brief Emit a JMP instruction with an undefined dummy-offset that must be patched later
  ///
  /// @param conditionCode Condition code for which to branch (CC::NONE for unconditional JMP)
  /// @return RelPatchObj RelPatchObj that will be used to patch the target
  RelPatchObj prepareJMP(CC const conditionCode = CC::NONE) const;

  ///
  /// @brief Emit a JMP instruction with an undefined dummy-offset that must be patched later if the given 64-bit GPR is
  /// zero
  ///
  /// @param reg 64-bit GPR to compare to zero
  /// @param is64 If a 64-bit comparison/register shall be used
  /// @return RelPatchObj RelPatchObj that will be used to patch the target
  RelPatchObj prepareJMPIfRegIsZero(REG const reg, bool const is64) const;

  ///
  /// @brief Emit a JMP instruction with an undefined dummy-offset that must be patched later if the given 64-bit GPR is
  /// not zero
  ///
  /// @param reg 64-bit GPR to compare to zero
  /// @param is64 If a 64-bit comparison/register shall be used
  /// @return RelPatchObj RelPatchObj that will be used to patch the target
  RelPatchObj prepareJMPIfRegIsNotZero(REG const reg, bool const is64) const;

  ///
  /// @brief Emit an ADR instruction with an undefined dummy-offset that must be patched later
  ///
  /// @param targetReg Register to put the resulting address into
  /// @return RelPatchObj RelPatchObj that will be used to patch the target
  RelPatchObj prepareADR(REG const targetReg) const;

  ///
  /// @brief Emits instructions that will raise a Wasm trap
  ///
  /// @param trapCode Identifier for the trap reason (will be passed to TrapException)
  /// If trapCode == TrapCode::NONE, trapCode in TrapReg will be used.
  void TRAP(TrapCode const trapCode) const;

  ///
  /// @brief Emits instructions that will conditionally raise a Wasm trap based on the conditionCode and the current CPU
  /// status flags
  ///
  /// @param trapCode Identifier for the trap reason (will be passed to TrapException)
  /// If trapCode == TrapCode::NONE, trapCode in TrapReg will be used.
  /// @param conditionCode Condition code in which case to trap
  void cTRAP(TrapCode const trapCode, CC const conditionCode) const;

  ///
  /// @brief Efficiently move an immediate value to a general purpose register
  ///
  /// Moves irrespective of what this register is currently containing, will not spill another register
  ///
  /// @param is64 Target register size
  /// @param reg Register to move the immediate to
  /// @param imm Immediate value to move to the register (upper 4 bytes will be ignored if is64 is false)
  void MOVimm(bool const is64, REG const reg, uint64_t const imm) const;

  ///
  /// @brief Efficiently move a 32-bit immediate value to a general purpose register
  ///
  /// @param reg Register to move the immediate to
  /// @param imm Immediate value to move to the register
  inline void MOVimm32(REG const reg, uint32_t const imm) const {
    MOVimm(false, reg, static_cast<uint64_t>(imm));
  }

  ///
  /// @brief Efficiently move a 64-bit immediate value to a general purpose register
  ///
  /// @param reg Register to move the immediate to
  /// @param imm Immediate value to move to the register
  inline void MOVimm64(REG const reg, uint64_t const imm) const {
    MOVimm(true, reg, imm);
  }

  ///
  /// @brief Moves an immediate float value (bit_cast to uint64_t) to a floating point register
  ///
  /// Moves irrespective of what this register is currently containing, will not spill another register.
  /// Not all immediate values can be directly moved into floating point registers on this architecture. Will not
  /// produce any machine code, but still confirm whether the move would've been successful in case reg == REG::NONE
  ///
  /// @param is64 Target register size
  /// @param reg Register to move the immediate to; pass REG::NONE if it should only be confirmed whether it can be
  /// directly moved, but not emit any machine code
  /// @param rawFloatImm Immediate floating point value to be moved (bit_cast to uint32_t or uint64_t, respectively and
  /// static_cast to uint64_t)
  /// @return bool Whether the move was successful and machine code has been emitted (if reg != REG::NONE)
  bool FMOVimm(bool const is64, REG const reg, uint64_t const rawFloatImm) const;

  ///
  /// @brief Adds a constant value to a general purpose register
  ///
  /// @param reg Which register to add the value to
  /// @param delta Signed value to add to the register
  /// @param is64 Whether this is a 64-bit operation
  /// @param protRegs Protected register mask
  /// @param intermReg Register to use if an intermediate scratch register is needed
  void addImmToReg(REG const reg, int64_t const delta, bool const is64, RegMask const protRegs = RegMask::none(), REG intermReg = REG::NONE) const;

  ///
  /// @brief Adds a constant value to a general purpose register
  ///
  /// @param dstReg Which register to store the add result
  /// @param srcReg Which register to store the add source
  /// @param delta Signed value to add to the register
  /// @param is64 Whether this is a 64-bit operation
  void addImmToReg(REG const dstReg, REG const srcReg, int64_t const delta, bool const is64) const;

  ///
  /// @brief Adds a constant value up to 2^24-1 (which can be encoded in 24 bits) to a general purpose register
  ///
  /// @param dstReg Which register to add the value to (or destination register if srcReg is explicitly given)
  /// @param delta Signed value to add to the register
  /// @param is64 Whether this is a 64-bit operation
  /// @param srcReg Optional source register, if REG::NONE is passed, dst is src
  void addImm24ToReg(REG const dstReg, int32_t const delta, bool const is64, REG srcReg = REG::NONE) const;

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
  Instruction INSTR(AbstrInstr const abstrInstr) const VB_NOEXCEPT;

  ///
  /// @brief Patch or modify the instruction in binary starting at a given offset
  ///
  /// @param binary Binary where this instruction is located in
  /// @param offset Offset at which the instruction starts in the binary
  /// @param lambda Function that can modify the lvalue instruction in place
  static void patchInstructionAtOffset(MemWriter &binary, uint32_t const offset, FunctionRef<void(Instruction &instruction)> const &lambda);

  /// @brief map to cache last trap instruction offset for each trap code
  class LastTrapPositionMap {
    std::array<uint32_t, static_cast<uint32_t>(TrapCode::MAX_TRAP_CODE) + 1U> data_; ///< array like map to storage last trap position.

  public:
    /// @brief get last trap instruction offset.
    /// @param trapCode target trap code.
    /// @param currentPosition current binary position.
    /// @param position output parameter, if return true, it is a valid last trap instruction offset.
    /// last trap instruction offset is instruction offset of load trapCode to TrapReg. If the trapCode ==
    /// TrapCode::NONE, instruction offset of trap.
    /// @return true if there are valid last trap JIT code postion.
    bool get(TrapCode const trapCode, uint32_t const currentPosition, uint32_t &position) const VB_NOEXCEPT {
      position = data_[static_cast<size_t>(trapCode)];
      return (position != 0U) && in_range<21U>(static_cast<int64_t>(currentPosition) - static_cast<int64_t>(position));
    }
    /// @brief get last trap instruction offset.
    /// @param trapCode target trap code.
    /// @param pos instruction offset of load trapCode to TrapReg. If the trapCode == TrapCode::NONE, instruction offset
    /// of trap.
    void set(TrapCode const trapCode, uint32_t const pos) VB_NOEXCEPT {
      data_[static_cast<size_t>(trapCode)] = pos;
    }
  };

  ///
  /// @brief Converts an ArgType to its MachineType
  ///
  /// @param argType Input ArgType
  /// @return MachineType
  static MachineType getMachineTypeFromArgType(ArgType const argType) VB_NOEXCEPT;

private:
  AArch64_Backend &backend_;                     ///< Reference to the backend instance
  MemWriter &binary_;                            ///< Reference to the output binary
  ModuleInfo &moduleInfo_;                       ///< Reference to the module info struct
  mutable LastTrapPositionMap lastTrapPosition_; ///< Trap code position. It can be reused to reduce code size

  ///
  /// @brief Determines whether an element matches a given ArgType
  ///
  /// e.g. A VariableStorage representing a 32-bit integer will only match a 32-bit integer ArgType and no 64-bit integer
  /// types or floating point ArgTypes
  ///
  /// @param argType ArgType to match
  /// @param storage VariableStorage to compare
  /// @return bool Whether the element and ArgType match
  bool elementFitsArgType(ArgType const argType, VariableStorage const &storage) const VB_NOEXCEPT;

  ///
  /// @brief Emits machine code (assembles the instruction) in the corresponding encoding for the given instruction and
  /// source and destination StackElements
  ///
  /// Caller must ensure that the instruction and its destination and sources match
  ///
  /// @param actionArg Abstract representation of an AArch64 instruction
  /// @param dest Representation of a register or memory location the instruction should use as destination
  /// @param src0 Representation of the first source of the instruction
  /// @param src1 Representation of the second source of the instruction, can be nullptr if the instruction allows
  void emitActionArg(AbstrInstr const actionArg, VariableStorage const &dest, VariableStorage const &src0, VariableStorage const &src1);
};

} // namespace aarch64
} // namespace vb

#endif /* A_ASM_H */

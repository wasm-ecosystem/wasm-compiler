///
/// @file tricore_assembler.hpp
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
#ifndef TRICORE_ASSEMBLER_HPP
#define TRICORE_ASSEMBLER_HPP

#include "src/config.hpp"
#include "src/core/compiler/common/StackElement.hpp"
#ifdef JIT_TARGET_TRICORE

#include <cstddef>

#include "tricore_encoding.hpp"
#include "tricore_instruction.hpp"

#include "src/core/common/FunctionRef.hpp"
#include "src/core/common/TrapCode.hpp"
#include "src/core/compiler/common/MachineType.hpp"
#include "src/core/compiler/common/ModuleInfo.hpp"
#include "src/core/compiler/common/RegMask.hpp"
#include "src/core/compiler/common/StackType.hpp"

namespace vb {

namespace tc {

class Tricore_Backend;

/// @brief jump condition instruction
class JumpCondition {
  static constexpr uint8_t NegateMask{0x1U}; ///< 2 kinds are pair, xor 0b1 will convert from one to the other

public:
  /// @brief jump condition kind
  enum class Kind : uint8_t {
    bitTrue,  ///< specific bit equal false (0)
    bitFalse, ///< specific bit equal true (1)

    I32LtReg, ///< i32 type less than register
    I32GeReg, ///< i32 type not less than register

    U32LtReg, ///< u32 type less than register
    U32GeReg, ///< u32 type not less than register

    I32LtConst4sx, ///< i32 type less than immediate number
    I32GeConst4sx, ///< i32 type not less than immediate number

    I32EqReg, ///< i32 equal register
    I32NeReg, ///< i32 not equal register

    AddrEqReg, ///< addr equal register
    AddrNeReg, ///< addr not equal register

    I32EqConst4sx, ///< i32 equal immediate number
    I32NeConst4sx, ///< i32 not equal immediate number
  };

  /// @brief create specific bit is 1 (true) jump condition
  /// @param reg register
  /// @param n position of the bit
  inline static JumpCondition bitTrue(REG const reg, SafeInt<4U> const n) VB_NOEXCEPT {
    return JumpCondition{Kind::bitTrue, reg, REG::NONE, n};
  }

  /// @brief create specific bit is 0 (false) jump condition
  /// @param reg register
  /// @param n position of the bit
  inline static JumpCondition bitFalse(REG const reg, SafeInt<4U> const n) VB_NOEXCEPT {
    return JumpCondition{Kind::bitFalse, reg, REG::NONE, n};
  }

  /// @brief create regA != regB jump condition
  /// @param regA register 1
  /// @param regB register 2
  inline static JumpCondition i32NeReg(REG const regA, REG const regB) VB_NOEXCEPT {
    return JumpCondition{Kind::I32NeReg, regA, regB, SafeInt<4U>::fromConst<0>()};
  }

  /// @brief create regA < regB (signed) jump condition
  /// @param regA register 1
  /// @param regB register 2
  inline static JumpCondition i32LtReg(REG const regA, REG const regB) VB_NOEXCEPT {
    return JumpCondition{Kind::I32LtReg, regA, regB, SafeInt<4U>::fromConst<0>()};
  }

  /// @brief create regA < regB (unsigned) jump condition
  /// @param regA register 1
  /// @param regB register 2
  inline static JumpCondition u32LtReg(REG const regA, REG const regB) VB_NOEXCEPT {
    return JumpCondition{Kind::U32LtReg, regA, regB, SafeInt<4U>::fromConst<0>()};
  }

  /// @brief create reg < imm (signed) jump condition
  /// @param reg register
  /// @param imm immediate number
  inline static JumpCondition i32LtConst4sx(REG const reg, SafeInt<4U> const imm) VB_NOEXCEPT {
    return JumpCondition{Kind::I32LtConst4sx, reg, REG::NONE, imm};
  }

  /// @brief create regA == regB jump condition
  /// @param regA addr register 1
  /// @param regB addr register 2
  inline static JumpCondition addrEqReg(REG const regA, REG const regB) VB_NOEXCEPT {
    return JumpCondition{Kind::AddrEqReg, regA, regB, SafeInt<4U>::fromConst<0>()};
  }

  /// @brief create reg == imm jump condition
  /// @param reg register
  /// @param imm immediate number
  inline static JumpCondition i32EqConst4sx(REG const reg, SafeInt<4U> const imm) VB_NOEXCEPT {
    return JumpCondition{Kind::I32EqConst4sx, reg, REG::NONE, imm};
  }
  /// @brief create reg != imm jump condition
  /// @param reg register
  /// @param imm immediate number
  inline static JumpCondition i32NeConst4sx(REG const reg, SafeInt<4U> const imm) VB_NOEXCEPT {
    return JumpCondition{Kind::I32NeConst4sx, reg, REG::NONE, imm};
  }

  /// @brief create negate jump condition
  inline JumpCondition negateJump() const VB_NOEXCEPT {
    JumpCondition conditionalJump{*this};
    // coverity[autosar_cpp14_a7_2_1_violation]
    conditionalJump.kind_ = static_cast<Kind>(static_cast<uint8_t>(NegateMask) ^ static_cast<uint8_t>(conditionalJump.kind_));
    return conditionalJump;
  }

  /// @brief create kind
  inline Kind getKind() const VB_NOEXCEPT {
    return kind_;
  }
  /// @brief create reg A
  inline REG getRegA() const VB_NOEXCEPT {
    return regA_;
  }
  /// @brief create reg B
  inline REG getRegB() const VB_NOEXCEPT {
    return regB_;
  }
  /// @brief create immediate number
  inline SafeInt<4U> getImm() const VB_NOEXCEPT {
    return imm_;
  }

private:
  Kind kind_;       ///< jump condition kind
  REG regA_;        ///< reg 1
  REG regB_;        ///< reg 2
  SafeInt<4U> imm_; ///< immediate number

  /// @brief constructor
  /// @param kind jump condition kind
  /// @param regA register 1
  /// @param regB register 2
  /// @param imm immediate number
  inline JumpCondition(Kind const kind, REG const regA, REG const regB, SafeInt<4U> const imm) VB_NOEXCEPT : kind_(kind),
                                                                                                             regA_(regA),
                                                                                                             regB_(regB),
                                                                                                             imm_(imm) {
  }
};

///
/// @brief tricore assembler class
///
class Tricore_Assembler final {
public:
  ///
  /// @brief Assembler class that emits machine code, controlled by the backend
  ///
  /// @param backend Reference to tricore backend
  /// @param binary Reference to output binary
  /// @param moduleInfo Reference to moduleInfo
  ///
  Tricore_Assembler(Tricore_Backend &backend, MemWriter &binary, ModuleInfo &moduleInfo) VB_NOEXCEPT;

  ///
  /// @brief Set the current function's stack frame size
  ///
  /// Includes temporary variables, local variables, return address and parameters in that order)
  ///
  /// @param frameSize New function frame size
  /// @param temporary Whether function frame size adjustment is only performed conditionally (temporary = true; e.g.
  /// wrapped in a conditional branch)
  /// @param mayRemoveLocals Whether this function can remove locals (e.g. right before a return)
  /// @param functionEntryAdjust if the function is entered by a fcall instruction, the stack need 4 bytes adjustment to make stack align at 8 because
  /// fcall pushes 4 bytes a11 on stack
  void setStackFrameSize(uint32_t const frameSize, bool const temporary = false, bool const mayRemoveLocals = false,
                         uint32_t const functionEntryAdjust = 0U);

  ///
  /// @brief Correctly align the stack frame size
  ///
  /// @param frameSize Stack frame size to align
  /// @return uint32_t Aligned stack frame size
  inline uint32_t alignStackFrameSize(uint32_t const frameSize) const VB_NOEXCEPT {
    // Align to 8B (without params)
    return roundUpToPow2(frameSize - moduleInfo_.fnc.paramWidth, 3U) + moduleInfo_.fnc.paramWidth;
  }

  ///
  /// @brief Check whether the stack pointer is below the stack fence; if so, trap with TrapCode::STACKFENCEBREACHED
  ///
  /// @param dataScrReg Data register to use as a scratch register
  /// @param addrScrReg Address register to use as a scratch register
  void checkStackFence(REG const dataScrReg, REG const addrScrReg) const;

  ///
  /// @brief Emits instructions that will raise a Wasm trap
  ///
  /// @param trapCode Identifier for the trap reason (will be passed to TrapException)
  void TRAP(TrapCode const trapCode) const;

  /// @brief prepare @b RelPatchObj according to jump condition.
  /// @param conditionJump jump condition.
  RelPatchObj prepareJump(JumpCondition const &conditionJump) const;

  ///
  /// @brief Emits instructions that will conditionally raise a Wasm trap based on the conditionJump
  ///
  /// @param trapCode Identifier for the trap reason (will be passed to TrapException)
  /// @param conditionJump Condition code in which case to trap
  void cTRAP(TrapCode const trapCode, JumpCondition const &conditionJump) const;

  /// @brief Prepared argument with a register, secondary register and a StackElement represented by that that can be
  /// used as input and output locations for instructions
  class PreparedArg final {
  public:
    StackElement elem;      ///< StackElement representing the location
    REG reg = REG::NONE;    ///< First register representing the location
    REG secReg = REG::NONE; ///< Secondary register representing the location
  };

  /// @brief Compound of a destination and two prepared argument args that can be used as input and output locations for
  /// instructions
  class PreparedArgs final {
  public:
    PreparedArg dest; ///< Where the output of the instruction can be placed
    PreparedArg arg0; ///< First input for the instruction
    PreparedArg arg1; ///< Second input for the instruction
  };

  ///
  /// @brief ...
  /// @param dstType ...
  /// @param arg0 First input for the instruction
  /// @param arg1 Second input for the instruction (can be nullptr if the instruction only has a single input)
  /// @param targetHint Optional target hint that can be used as a scratch register if it is appropiate (Must be nullptr
  /// for readonly instructions like CMP; can optionally be nullptr in all other cases)
  /// @param protRegs Protected register mask (i.e. which registers not to use)
  /// @param forceDstArg0Diff Force the operation to not use the first argument location as destination location
  /// @param forceDstArg1Diff Force the operation to not use the second argument location as destination location
  /// @return StackElement Indicating where the result of the instruction is now stored
  PreparedArgs loadArgsToRegsAndPrepDest(MachineType const dstType, StackElement const *const arg0, StackElement const *const arg1,
                                         StackElement const *const targetHint = nullptr, RegMask const protRegs = RegMask::none(),
                                         bool const forceDstArg0Diff = false, bool const forceDstArg1Diff = false) const;

  ///
  /// @brief Selects an instruction for input StackElements from an array of abstract instructions and writes machine
  /// code to the output binary
  ///
  /// @param instructions Reference to an array of instructions.
  /// @param arg0 first argument
  /// @param arg1 second argument, could be nullptr if unop
  /// @param targetHint Optional target hint that can be used as a scratch register if it is appropiate (Must be nullptr
  /// for readonly instructions like CMP; can optionally be nullptr in all other cases)
  /// @param protRegs Protected register mask (i.e. which registers not to use)
  /// @return result StackElement
  /// @throws vb::RuntimeError If not enough memory is available
  StackElement selectInstr(Span<AbstrInstr const> const &instructions, StackElement const *const arg0, StackElement const *const arg1,
                           StackElement const *const targetHint, RegMask const protRegs) VB_THROW;

  ///
  /// @brief Wrapper for selectInstr
  ///
  /// @tparam N Size of Array
  /// @param instructions see @b selectInstr
  /// @param arg0 see @b selectInstr
  /// @param arg1 see @b selectInstr
  /// @param targetHint see @b selectInstr
  /// @param protRegs see @b selectInstr
  /// @return see @b selectInstr
  /// @throws see @b selectInstr
  template <size_t N>
  inline StackElement selectInstr(std::array<AbstrInstr const, N> const &instructions, StackElement const *const arg0, StackElement const *const arg1,
                                  StackElement const *const targetHint, RegMask const protRegs) VB_THROW {
    return selectInstr(Span<AbstrInstr const>{instructions.data(), instructions.size()}, arg0, arg1, targetHint, protRegs);
  }

  ///
  /// @brief Patch or modify the instruction in binary starting at a given offset
  ///
  /// @param binary Binary where this instruction is located in
  /// @param offset Offset at which the instruction starts in the binary
  /// @param lambda Function that can modify the lvalue instruction in place
  static void patchInstructionAtOffset(MemWriter &binary, uint32_t const offset, FunctionRef<void(Instruction &instruction)> const &lambda);

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

  ///
  /// @brief Efficiently move an immediate value to a general purpose register
  ///
  /// Moves irrespective of what this register is currently containing, will not spill another register
  ///
  /// @param reg Register to move the immediate to
  /// @param imm Immediate value to move to the register
  void MOVimm(REG const reg, uint32_t const imm) const;

  /// @brief DReg = M(AReg + disp, word)
  /// @param dataReg DReg in formula
  /// @param addrReg AReg in formula
  /// @param disp disp in formula, max off16sx
  void loadWordDRegDerefARegDisp16sx(REG const dataReg, REG const addrReg, SafeInt<16U> const disp) const;

  /// @brief DReg = M(AReg + disp, byte) - load byte unsigned
  /// @param dataReg DReg in formula
  /// @param addrReg AReg in formula
  /// @param disp disp in formula, max off16sx
  void loadByteUnsignedDRegDerefARegDisp16sx(REG const dataReg, REG const addrReg, SafeInt<16U> const disp) const;

  /// @brief DReg = M(AReg + disp, halfword) - load halfword signed
  /// @param dataReg DReg in formula
  /// @param addrReg AReg in formula
  /// @param disp disp in formula, max off16sx
  void loadHalfwordDRegDerefARegDisp16sx(REG const dataReg, REG const addrReg, SafeInt<16U> const disp) const;

  /// @brief M(AReg + disp, byte) = DReg
  /// @param dataReg DReg in formula
  /// @param addrReg AReg in formula
  /// @param disp disp in formula, max off16sx
  void storeByteDerefARegDisp16sxDReg(REG const dataReg, REG const addrReg, SafeInt<16U> const disp) const;

  /// @brief M(AReg + disp, halfword) = DReg
  /// @param dataReg DReg in formula
  /// @param addrReg AReg in formula
  /// @param disp disp in formula, max off16sx
  void storeHalfwordDerefARegDisp16sxDReg(REG const dataReg, REG const addrReg, SafeInt<16U> const disp) const;

  /// @brief M(AReg + disp, word) = DReg
  /// @param dataReg DReg in formula
  /// @param addrReg AReg in formula
  /// @param disp disp in formula, max off16sx
  void storeWordDerefARegDisp16sxDReg(REG const dataReg, REG const addrReg, SafeInt<16U> const disp) const;

  ///
  /// @brief Adds a constant value to a data register
  /// @details Effectively performs: targetReg = reg + imm, will not modify reg if targetReg is set. reg will be used as
  /// target if targetReg is not set (or set to REG::NONE)
  ///
  /// @param reg Which register to add the value to
  /// @param imm Value to add to the register
  /// @param targetReg Register to use as destination, needs to be of the same type (A/D) as reg
  /// @throws vb::RuntimeError If not enough memory is available
  void addImmToReg(REG const reg, uint32_t const imm, REG targetReg = REG::NONE) const;

  /// @brief sp = sp - imm
  /// @param imm imm
  void subSp(uint32_t const imm) const;

  ///
  /// @brief Load a PC-relative address (linked to the resulting RelPatchObj) to a register
  ///
  /// @param addrTargetReg Register to load the absolute address into
  /// @param addrScratchReg Scratch register to temporarily store A[11] in, if REG::NONE is passed, A[11] will be
  /// clobbered
  /// @return RelPatchObj that should be linked to the target position
  RelPatchObj loadPCRelAddr(REG const addrTargetReg, REG const addrScratchReg) const;

  ///
  /// @brief emit EQ_Dc_Da_const9sx instruction, use 16-bit instruction instead if possible
  ///
  /// @param Dc Dc filed reg
  /// @param Da Da filed reg
  /// @param const9 const9 filed imm
  ///
  inline void eqWordDcDaConst9sx(REG const Dc, REG const Da, SafeInt<9> const const9) const {
    emitDcDaConst9sx(EQ_D15_Da_const4sx, EQ_Dc_Da_const9sx, Dc, Da, const9);
  }

  ///
  /// @brief emit EQ_Dc_Da_Db instruction, use 16-bit instruction instead if possible
  ///
  /// @param Dc Dc filed reg
  /// @param Da Da filed reg
  /// @param Db Db filed reg
  ///
  inline void eqWordDcDaDb(REG const Dc, REG const Da, REG const Db) const {
    emitDcDaDb(EQ_D15_Da_Db, EQ_Dc_Da_Db, Dc, Da, Db);
  }

  /// @brief emit LT_Dc_Da_const9sx instruction, use 16-bit instruction instead if possible
  /// @param Dc Dc filed reg
  /// @param Da Da filed reg
  /// @param const9 const9 filed imm
  inline void ltWordDcDaConst9sx(REG const Dc, REG const Da, SafeInt<9> const const9) const {
    emitDcDaConst9sx(LT_D15_Da_const4sx, LT_Dc_Da_const9sx, Dc, Da, const9);
  }

  ///
  /// @brief emit LT_Dc_Da_Db instruction, use 16-bit instruction instead if possible
  ///
  /// @param Dc Dc filed reg
  /// @param Da Da filed reg
  /// @param Db Db filed reg
  ///
  inline void ltWordDcDaDb(REG const Dc, REG const Da, REG const Db) const {
    emitDcDaDb(LT_D15_Da_Db, LT_Dc_Da_Db, Dc, Da, Db);
  }

  /// @brief emit AND_Dc_Da_const9zx instruction, use 16-bit instruction instead if possible
  /// @param Dc Dc filed reg
  /// @param Da Da filed reg
  /// @param const9 const9 filed imm
  inline void andWordDcDaConst9zx(REG const Dc, REG const Da, SafeUInt<9U> const const9) const {
    emitDcDaConst9zx(AND_D15_const8zx, AND_Dc_Da_const9zx, Dc, Da, const9);
  }

  /// @brief emit OR_Dc_Da_const9zx instruction, use 16-bit instruction instead if possible
  /// @param Dc Dc filed reg
  /// @param Da Da filed reg
  /// @param const9 const9 filed imm
  inline void orWordDcDaConst9zx(REG const Dc, REG const Da, SafeUInt<9U> const const9) const {
    emitDcDaConst9zx(OR_D15_const8zx, OR_Dc_Da_const9zx, Dc, Da, const9);
  }

  ///
  /// @brief select Dc_Da_Db instruction if the instruction has a 16-bit variant using D15 as implicit target
  ///
  /// @param instruction16 the 16 bit opcode template
  /// @param instruction32 the 32 bit opcode template
  /// @param Dc Dc filed reg
  /// @param Da Da filed reg
  /// @param Db Db filed reg
  ///
  void emitDcDaDb(OPCodeTemplate const instruction16, OPCodeTemplate const instruction32, REG const Dc, REG const Da, REG const Db) const;

  ///
  /// @brief select Dc_Da_Const9sx instruction if the instruction has a 16-bit variant using D15 as implicit target
  ///
  /// @param instruction16 the 16 bit opcode template
  /// @param instruction32 the 32 bit opcode template
  /// @param Dc Dc filed reg
  /// @param Da Da filed reg
  /// @param const9 const9 filed imm
  void emitDcDaConst9sx(OPCodeTemplate const instruction16, OPCodeTemplate const instruction32, REG const Dc, REG const Da,
                        SafeInt<9> const const9) const;

  ///
  /// @brief select Dc_Da_Const8zx instruction if the instruction has a 16-bit variant using D15 as implicit target
  ///
  /// @param instruction16 the 16 bit opcode template
  /// @param instruction32 the 32 bit opcode template
  /// @param Dc Dc filed reg
  /// @param Da Da filed reg
  /// @param const9 const8 filed imm
  void emitDcDaConst9zx(OPCodeTemplate const instruction16, OPCodeTemplate const instruction32, REG const Dc, REG const Da,
                        SafeUInt<9> const const9) const;

  ///
  /// @brief select 16-bit LDA_Ac_deref_Ab instruction if the offset is zero. Otherwise select LDA_Aa_deref_Ab_off16sx. If offset is a compile-time
  /// constant, you can consider select ins directly to reduce the extra `if` overhead at runtime.
  ///
  /// @param desDataReg register to store the data that load gets
  /// @param addrBaseReg Ab filed reg
  /// @param offset16 Offset of the load address relative to start
  void emitLoadDerefOff16sx(REG const desDataReg, REG const addrBaseReg, SafeInt<16> const offset16) const;

  ///
  /// @brief select 16-bit STA_deref_Ab_Aa instruction if the offset is zero. Otherwise select STA_deref_Ab_off16sx_Aa
  ///
  /// @param addrBaseReg Ab field reg
  /// @param srcDataReg data register need to store
  /// @param offset16 Offset of the store address relative to start
  void emitStoreDerefOff16sx(REG const addrBaseReg, REG const srcDataReg, SafeInt<16> const offset16) const;

  ///
  /// @brief Converts an ArgType to its MachineType
  ///
  /// @param argType Input ArgType
  /// @return MachineType
  static MachineType getMachineTypeFromArgType(ArgType const argType) VB_NOEXCEPT;

  ///
  /// @brief operand movement
  ///
  struct OperandMovement final {
    uint32_t cost;      ///< Total cost in bytes
    uint32_t liftCount; ///< Count of operand lift operation
    bool movArg0;       ///< Whether mov arg0
    bool movArg1;       ///< Whether mov arg1
    bool reversed;      ///< Whether the arguments were swapped (only possible if the input sources are set commutative)
  };

private:
  /// @brief Invalid mov cost
  static constexpr uint32_t invalidMovCost{UINT32_MAX};
  /// @brief map to cache last trap JIT code position for each trap code
  class LastTrapPositionMap {
    std::array<uint32_t, static_cast<uint32_t>(TrapCode::MAX_TRAP_CODE) + 1U> data_; ///< array like map to storage last trap position.

  public:
    /// @brief get last trap JIT code position.
    /// @param trapCode target trap code.
    /// @param currentPosition current binary position.
    /// @param position output parameter, if return true, it is a valid last trap JIT code position.
    /// @return true if there are valid last trap JIT code postion.
    template <size_t bits_target>
    SignedInRangeCheck<bits_target> get(TrapCode const trapCode, uint32_t const currentPosition, uint32_t &position) const VB_NOEXCEPT {
      position = data_[static_cast<size_t>(trapCode)];
      if (position != 0U) {
        return SignedInRangeCheck<bits_target>::check(static_cast<int64_t>(position) - static_cast<int64_t>(currentPosition));
      } else {
        return SignedInRangeCheck<bits_target>::invalid();
      }
    }
    /// @brief get last trap JIT code position.
    /// @param trapCode target trap code.
    /// @param pos mov trapCode to trap register instruction position.
    void set(TrapCode const trapCode, uint32_t const pos) VB_NOEXCEPT {
      data_[static_cast<size_t>(trapCode)] = pos;
    }
  };

  Tricore_Backend &backend_;                     ///< Reference to the backend instance
  MemWriter &binary_;                            ///< Reference to the output binary
  ModuleInfo &moduleInfo_;                       ///< Reference to the module info struct
  mutable LastTrapPositionMap lastTrapPosition_; ///< Trap code position. It can be reused to reduce code size

  ///
  /// @brief Emits machine code (assembles the instruction) in the corresponding encoding for the given instruction and
  /// source and destination StackElements
  ///
  /// Caller must ensure that the instruction and its destination and sources match
  ///
  /// @param abstrInstr Abstract representation of an AArch64 instruction
  /// @param dest Representation of a register or memory location the instruction should use as destination
  /// @param src0 Representation of the first source of the instruction
  /// @param src1 Representation of the second source of the instruction, can be nullptr if the instruction allows
  void emitAbstrInstr(AbstrInstr const &abstrInstr, VariableStorage const &dest, VariableStorage const &src0, VariableStorage const &src1) VB_THROW;

  ///
  /// @brief Get operand movement cost (instructions size in bytes)
  ///
  /// @param argType Destination ArgType
  /// @param storage Source VariableStorage
  /// @return uint32_t Byte length of the instructions required to move the operand
  uint32_t getOperandMovCost(ArgType const argType, VariableStorage const &storage) const VB_THROW;

  ///
  /// @brief Determines whether an element matches a given ArgType
  ///
  /// e.g. A VariableStorage representing a 5-bit constant can match a const8zx ArgType and no const4zx ArgType
  ///
  /// @param argType ArgType to match
  /// @param storage VariableStorage to compare
  /// @return bool Whether the element and ArgType match
  inline bool elementFitsArgType(ArgType const argType, VariableStorage const &storage) const VB_THROW {
    return (getOperandMovCost(argType, storage) == 0U);
  }

  ///
  /// @brief Check whether the storage needs to be moved to argType with an additional move instruction
  ///
  /// @param argType ArgType
  /// @param storage VariableStorage
  /// @return bool Whether need an addition move instruction
  inline bool needMoveOperand(ArgType const argType, VariableStorage const &storage) const VB_THROW {
    uint32_t const cost{getOperandMovCost(argType, storage)};
    return (cost != 0U) && (cost != invalidMovCost);
  }

  ///
  /// @brief Get instruction cost (instructions size in bytes)
  ///
  /// @param instruction Abstract instruction
  /// @param arg0 Variable storage of first argument
  /// @param arg1 Variable storage of second argument
  /// @param startedAsWritableScratchReg whether reg in input storage is writable scratch register.
  /// @param verifiedTargetHint Variable storage of verified targetHint
  /// @param isD15Available Whether D15 is available
  /// @return OperandMovement Byte length of the instructions require to move the operand
  OperandMovement getInstructionCost(AbstrInstr const &instruction, VariableStorage const &arg0, VariableStorage const &arg1,
                                     std::array<bool const, 3> const startedAsWritableScratchReg, VariableStorage const &verifiedTargetHint,
                                     bool const isD15Available) const VB_THROW;

  ///
  /// @brief Check if given argType is a 32-bit data register, the suffix _a, _b, _c is used to specify the operand location
  /// @return bool
  bool isDataReg32(ArgType const argType) const VB_NOEXCEPT {
    return ((argType == ArgType::dataReg32_a) || (argType == ArgType::dataReg32_b)) || (argType == ArgType::dataReg32_c);
  }
};

} // namespace tc
} // namespace vb

#endif // JIT_TARGET_TRICORE

#endif // TRICORE_ASSEMBLER_HPP

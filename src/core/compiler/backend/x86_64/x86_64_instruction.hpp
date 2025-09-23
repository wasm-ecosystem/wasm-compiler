///
/// @file x86_64_instruction.hpp
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
#ifndef X86_64_INSTRUCTION_HPP
#define X86_64_INSTRUCTION_HPP

#include <cstdint>

#include "x86_64_encoding.hpp"

#include "src/core/common/util.hpp"
#include "src/core/compiler/common/MemWriter.hpp"

namespace vb {
namespace x86_64 {

///
/// @brief Instruction class used to assemble and encode a specific AArch64 instruction and then write it to an output
/// binary
///
class Instruction final {
public:
  ///
  /// @brief Construct a new Instruction instance
  ///
  /// @param opcode Basic opcode template
  /// @param binary Reference to the output binary
  Instruction(OPCodeTemplate const opcode, MemWriter &binary) VB_NOEXCEPT;

  ///
  /// @brief Construct a new Instruction instance
  ///
  /// @param abstrInstr Abstract instruction
  /// @param binary Reference to the output binary
  Instruction(AbstrInstr const &abstrInstr, MemWriter &binary) VB_NOEXCEPT;
  Instruction(Instruction &) = delete;

  ///
  /// @brief Move constructor
  ///
  Instruction(Instruction &&) = default;
  Instruction &operator=(const Instruction &) & = delete;
  Instruction &operator=(Instruction &&) & = delete;
  ~Instruction() VB_NOEXCEPT;

  ///
  /// @brief Set a register to the r field
  ///
  /// @param reg Register
  /// @return Instruction& Reference to the instruction
  Instruction &setR(REG const reg) VB_NOEXCEPT;

  ///
  /// @brief Set a register to the r/m field
  ///
  /// @param reg Register
  /// @return Instruction& Reference to the instruction
  Instruction &setR4RM(REG const reg) VB_NOEXCEPT;

  ///
  /// @brief Set a 8 bit register to the r/m field
  ///
  /// @param reg Register
  /// @return Instruction& Reference to the instruction
  Instruction &setR8_4RM(REG const reg) VB_NOEXCEPT;

  ///
  /// @brief Set a memory location to the r/m field
  ///
  /// @param baseReg Base register
  /// @param displacement Displacement for memory access in bytes
  /// @param indexReg Index register
  /// @param indexScalePow2 Scale factor for indexReg as power of two (0=1x, 1=2x, 2=4x, 3=8x)
  /// @return Instruction& Reference to the instruction
  Instruction &setM4RM(REG const baseReg, int32_t const displacement, REG const indexReg = REG::NONE, uint32_t const indexScalePow2 = 0U) VB_NOEXCEPT;

  ///
  /// @brief Set a 8 bit memory location to the r/m field
  ///
  /// @param baseReg Base register
  /// @param displacement Displacement for memory access in bytes
  /// @param indexReg Index register
  /// @param indexScalePow2 Scale factor for indexReg as power of two (0=1x, 1=2x, 2=4x, 3=8x)
  /// @return Instruction& Reference to the instruction
  Instruction &setM8_4RM(REG const baseReg, int32_t const displacement, REG const indexReg = REG::NONE,
                         uint32_t const indexScalePow2 = 0U) VB_NOEXCEPT;

  ///
  /// @brief Set a memory location as offset from the program counter at the start of this instruction to the r/m field
  ///
  /// @param displacementFromInstructionStart Displacement from the start of the instruction
  /// @return Instruction& Reference to the instruction
  Instruction &setMIP4RM(int32_t const displacementFromInstructionStart) VB_NOEXCEPT;

  ///
  /// @brief Set a memory location as absolute position in the output binary to the r/m field
  ///
  /// Convenience wrapper for setMIP4RM that calculates the offset
  ///
  /// @param binaryPosition Position in the output binary to dereference
  /// @return Instruction& Reference to the instruction
  Instruction &setMIP4RMabs(uint32_t const binaryPosition) VB_NOEXCEPT;

  ///
  /// @brief Set an 8-bit immediate to the imm8 field
  ///
  /// @param imm 8-bit immediate
  /// @return Instruction& Reference to the instruction
  Instruction &setImm8(uint8_t const imm) VB_NOEXCEPT;

  ///
  /// @brief Set a 16-bit immediate to the imm16 field
  ///
  /// @param imm 16-bit immediate
  /// @return Instruction& Reference to the instruction
  Instruction &setImm16(uint16_t const imm) VB_NOEXCEPT;

  ///
  /// @brief Set a 32-bit immediate to the imm32 field
  ///
  /// @param imm 32-bit immediate
  /// @return Instruction& Reference to the instruction
  Instruction &setImm32(uint32_t const imm) VB_NOEXCEPT;

  ///
  /// @brief Set a 64-bit immediate to the imm64 field
  ///
  /// @param imm 64-bit immediate
  /// @return Instruction& Reference to the instruction
  Instruction &setImm64(uint64_t const imm) VB_NOEXCEPT;

  ///
  /// @brief Set an 8-bit relative offset to the rel8 field
  ///
  /// @param rel 8-bit relative offset
  /// @return Instruction& Reference to the instruction
  Instruction &setRel8(int8_t const rel) VB_NOEXCEPT;

  ///
  /// @brief Set a 32-bit relative offset to the rel32 field
  ///
  /// @param rel 32-bit relative offset
  /// @return Instruction& Reference to the instruction
  Instruction &setRel32(int32_t const rel) VB_NOEXCEPT;

  ///
  /// @brief Set the condition code
  ///
  /// @param ccIn Condition code
  /// @return Instruction&  Reference to the instruction
  Instruction &setCC(CC const ccIn) VB_NOEXCEPT;

  ///
  /// @brief Assembles the instruction and writes it to the output memory
  ///
  /// @throws std::range_error If not enough memory is available
  void emitCode();

  ///
  /// @brief Assembles the instruction and writes it to the output memory
  ///
  /// Shortcut for emitCode()
  ///
  /// @throws std::range_error If not enough memory is available
  inline void operator()() {
    emitCode();
  };

private:
  ///
  /// @brief Which type of immediate to encode
  ///
  enum class ImmType : uint8_t { NONE = 0, IMM8 = 1, IMM16 = 2, IMM32 = 4, IMM64 = 8 };

  ///
  /// @brief Which access type to encode for the r/m field
  ///
  enum class RMType : uint8_t {
    NONE,
    MEM_RIP_DISPFROMINSTRSTART, // add rax, [rip - 4]
    MEM,                        // add rax, [rbx + rax * 4 + 4] or add rax, [rbx + 4] or add rax, [rbx]
    REG                         // add rax, rbx
  };

  OPCodeTemplate opcode_;     ///< Basic opcode template
  CC cc_;                     ///< Condition code for the instruction
  RMType rmType_;             ///< Which access type to encode for the r/m
  ImmType immType_;           ///< Which type immediate to encode
  uint64_t immediate_;        ///< Zero-extended immediate
  REG rReg_;                  ///< Index of reg for r field
  REG rmBaseReg_;             ///< Index of reg for base register (memory access)
  REG rmIndexReg_;            ///< Index of reg for r/m field
  uint32_t rmIndexScalePow2_; ///< Scale factor for a memory access
  int32_t rmDisplacement_;    ///< Displacement for a memory access

  MemWriter &binary_; ///< Output binary

  bool emitted_; ///< Whether this instruction has been emitted yet
};

} // namespace x86_64
} // namespace vb

#endif

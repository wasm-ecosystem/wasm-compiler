///
/// @file DwarfImpl.cpp
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
#include <array>
#include <cassert>
#include <cstdint>
#include <iostream>
#include <ostream>
#include <string_view>
#include <vector>

#include "DwarfDebugInfo.hpp"
#include "DwarfImpl.hpp"

#include "src/config.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/common/StackElement.hpp"

namespace vb {
namespace extension {

namespace {

enum class DW_TAG : uint32_t {
  compile_unit = 0x11U, ///< DW_TAG_compile_unit
  subprogram = 0x2EU,   ///< DW_TAG_subprogram
};

enum class DW_AT : uint32_t {
  low_pc = 0x11U,  ///< DW_AT_low_pc
  high_pc = 0x12U, ///< DW_AT_high_pc
};

enum class DW_FORM : uint32_t {
  addr = 0x01U,         ///< DW_FORM_addr
  string = 0x08U,       ///< DW_FORM_string
  data4 = 0x05U,        ///< DW_FORM_data4
  data8 = 0x07U,        ///< DW_FORM_data8
  flag_present = 0x0CU, ///< DW_FORM_flag_present
};

enum class DW_Lnct : uint32_t {
  path = 0x01U, ///< DW_LNCT_path
};

} // namespace

static DebugLineOpCode createAdvancePC(uint32_t const offset) VB_NOEXCEPT {
  DebugLineOpCode op{};
  op.kind_ = DebugLineOpCode::OpCodeKind::advance_pc;
  op.v_.advancePC_.offset_ = offset;
  return op;
}
static DebugLineOpCode createAdvanceLine(int32_t const offset) VB_NOEXCEPT {
  DebugLineOpCode op{};
  op.kind_ = DebugLineOpCode::OpCodeKind::advance_line;
  op.v_.advanceLine_.offset_ = offset;
  return op;
}
static DebugLineOpCode createCopy() VB_NOEXCEPT {
  DebugLineOpCode op{};
  op.kind_ = DebugLineOpCode::OpCodeKind::copy;
  return op;
}

static void pushStr(std::string_view str, std::vector<uint8_t> &result) {
  result.insert(result.end(), str.begin(), str.end());
  result.push_back('\0');
}
template <class T> static void pushLE32(T const value, std::vector<uint8_t> &result) {
  uint32_t const v = static_cast<uint32_t>(value);
  result.push_back(static_cast<uint8_t>(v >> 0U));
  result.push_back(static_cast<uint8_t>(v >> 8U));
  result.push_back(static_cast<uint8_t>(v >> 16U));
  result.push_back(static_cast<uint8_t>(v >> 24U));
}
template <class T> static void pushLE64(T const value, std::vector<uint8_t> &result) {
  uint64_t const v = static_cast<uint64_t>(value);
  result.push_back(static_cast<uint8_t>(v >> 0U));
  result.push_back(static_cast<uint8_t>(v >> 8U));
  result.push_back(static_cast<uint8_t>(v >> 16U));
  result.push_back(static_cast<uint8_t>(v >> 24U));
  result.push_back(static_cast<uint8_t>(v >> 32U));
  result.push_back(static_cast<uint8_t>(v >> 40U));
  result.push_back(static_cast<uint8_t>(v >> 48U));
  result.push_back(static_cast<uint8_t>(v >> 56U));
}
template <class T> static void writeLE32(T const value, std::vector<uint8_t> &result, size_t const offset) {
  assert(offset + 4U <= result.size() && "Offset out of bounds for writeLE32");
  uint32_t const v = static_cast<uint32_t>(value);
  result[offset + 0U] = static_cast<uint8_t>(v >> 0U);
  result[offset + 1U] = static_cast<uint8_t>(v >> 8U);
  result[offset + 2U] = static_cast<uint8_t>(v >> 16U);
  result[offset + 3U] = static_cast<uint8_t>(v >> 24U);
}

static void encodeULEB128(uint32_t value, std::vector<uint8_t> &bytes) {
  do {
    uint8_t byte = value & 0x7FU;
    value >>= 7U;
    if (value != 0U) {
      byte |= 0x80U;
    }
    bytes.push_back(byte);
  } while (value != 0U);
}

static void encodeSLEB128(int32_t value, std::vector<uint8_t> &bytes) {
  bool more = true;
  while (more) {
    uint8_t byte = static_cast<uint8_t>(value) & 0x7FU;
    value = value >> 7; // NOLINT(hicpp-signed-bitwise)
    more = ((value != 0) || ((byte & 0x40U) != 0)) && ((value != -1) || ((byte & 0x40U) == 0));
    if (more) {
      byte |= 0x80U;
    }
    bytes.push_back(byte);
  }
}

static void toDwarf5FormatImpl(DebugLineOpCode const &op, std::vector<uint8_t> &result) {
  switch (op.kind_) {
  case DebugLineOpCode::OpCodeKind::advance_pc: {
    // DW_LNS_advance_pc opcode: 0x02, followed by ULEB128 operand
    result.push_back(0x02U);
    encodeULEB128(static_cast<uint32_t>(op.v_.advancePC_.offset_), result);
    break;
  }
  case DebugLineOpCode::OpCodeKind::advance_line: {
    // DW_LNS_advance_line opcode: 0x03, followed by SLEB128 operand
    result.push_back(0x03U);
    encodeSLEB128(op.v_.advanceLine_.offset_, result);
    break;
  }
  case DebugLineOpCode::OpCodeKind::copy: {
    // DW_LNS_copy opcode: 0x01
    result.push_back(0x01U);
    break;
  }
  default:
    UNREACHABLE(, "Unsupported DebugLineOpCode kind: " << static_cast<uint8_t>(op.kind_));
    break;
  }
}
static void dumpImpl(DebugLine const &debugLine, std::ostream &os) {
  struct StateMachine {
    uint32_t address_;
    uint32_t line_; ///< wasm bytecode offset
  };
  StateMachine state{0U, 0U};
  for (const DebugLineOpCode &op : debugLine.sequences_) {
    switch (op.kind_) {
    case DebugLineOpCode::OpCodeKind::advance_pc:
      state.address_ += op.v_.advancePC_.offset_;
      os << "DW_LNS_advance_pc: " << op.v_.advancePC_.offset_ << std::endl;
      break;
    case DebugLineOpCode::OpCodeKind::advance_line:
      state.line_ += static_cast<uint32_t>(op.v_.advanceLine_.offset_);
      os << "DW_LNS_advance_line: " << op.v_.advanceLine_.offset_ << std::endl;
      break;
    case DebugLineOpCode::OpCodeKind::copy:
      os << "DW_LNS_copy" << " " << state.address_ << " -> " << state.line_ << std::endl;
      break;
    default:
      UNREACHABLE(, "Unsupported DebugLineOpCode kind: " << static_cast<uint8_t>(op.kind_));
      break;
    }
  }
}

static void createDebugLineSection(DebugLine const &debugLine, std::vector<uint8_t> &result) {
  // DWARF v5 Line Number Program Header
  // See DWARF v5 spec, section 6.2.4

  // Reserve space for unit_length (4 bytes, will fill later)
  size_t const unitLengthOffset = result.size();
  result.resize(result.size() + 4U, 0U);

  // version (2 bytes)
  result.push_back(0x05U);
  result.push_back(0x00U);

  // address_size (1 byte)
  constexpr uint8_t addressSize = static_cast<uint8_t>(8U);
  result.push_back(addressSize);

  // segment_selector_size (1 byte)
  constexpr uint8_t segmentSelectorSize = static_cast<uint8_t>(0U);
  result.push_back(segmentSelectorSize);

  // header_length (4 bytes, will fill later)
  size_t const headerLengthOffset = result.size();
  result.resize(result.size() + 4U, 0U);

  // minimum_instruction_length (1 byte)
  constexpr uint8_t minimum_instruction_length = static_cast<uint8_t>(1U);
  result.push_back(minimum_instruction_length);

  // maximum_operations_per_instruction (1 byte)
  constexpr uint8_t maximum_operations_per_instruction = static_cast<uint8_t>(1U);
  result.push_back(maximum_operations_per_instruction);

  // default_is_stmt (1 byte)
  constexpr uint8_t defaultIsStmt = static_cast<uint8_t>(1U);
  result.push_back(defaultIsStmt);

  // line_base (1 byte, signed)
  constexpr uint8_t line_base = static_cast<uint8_t>(-3);
  result.push_back(line_base);
  // line_range (1 byte)
  constexpr uint8_t line_range = static_cast<uint8_t>(14U);
  result.push_back(line_range);

  /// @brief standard_opcode_lengths (opcode_base - 1 bytes)
  /// According to DWARF v5, section 6.2.5.1
  enum class DW_LNS : uint8_t { extended = 0U, copy = 1U, advance_pc = 2U, advance_line = 3U, set_file = 4U, max = set_file };
  result.push_back(static_cast<uint8_t>(DW_LNS::max) + 1U);

  // standard_opcode_lengths (opcode_base - 1 bytes)
  //  1: DW_LNS_copy
  //  2: DW_LNS_advance_pc (1 ULEB128)
  //  3: DW_LNS_advance_line (1 SLEB128)
  //  4: DW_LNS_set_file (1 ULEB128)
  //  5: DW_LNS_set_column (1 ULEB128)
  //  6: DW_LNS_negate_stmt (0)
  //  7: DW_LNS_set_basic_block (0)
  //  8: DW_LNS_const_add_pc (0)
  //  9: DW_LNS_fixed_advance_pc (1 uhalf)
  // 10: DW_LNS_set_prologue_end (0)
  // 11: DW_LNS_set_epilogue_begin (0)
  // 12: DW_LNS_set_isa (1 ULEB128)
  std::array<uint8_t, static_cast<size_t>(DW_LNS::max)> const standard_opcode_lengths = {
      0, // DW_LNS_copy
      1, // DW_LNS_advance_pc
      1, // DW_LNS_advance_line
      1, // DW_LNS_set_file
  };
  for (uint8_t const standardOpcodeLength : standard_opcode_lengths) {
    result.push_back(standardOpcodeLength);
  }

  constexpr uint8_t directory_entry_format_count = static_cast<uint8_t>(1U);
  result.push_back(directory_entry_format_count);
  result.push_back(static_cast<uint8_t>(DW_Lnct::path));
  result.push_back(static_cast<uint8_t>(DW_FORM::string));
  constexpr uint8_t directories_count = static_cast<uint8_t>(1U);
  result.push_back(directories_count);
  result.push_back('t');
  result.push_back('m');
  result.push_back('p');
  result.push_back(0x00U);
  constexpr uint8_t file_name_entry_format_count = static_cast<uint8_t>(1U);
  result.push_back(file_name_entry_format_count);
  result.push_back(static_cast<uint8_t>(DW_Lnct::path));
  result.push_back(static_cast<uint8_t>(DW_FORM::string));
  constexpr uint8_t file_names_count = static_cast<uint8_t>(1U);
  result.push_back(file_names_count);
  if (debugLine.fileName_.empty()) {
    pushStr(".wasm", result);
  } else {
    pushStr(debugLine.fileName_, result);
  }
  // Now, program header ends here
  size_t const headerEnd = result.size();

  // Emit the line number program (instructions)
  // Set initial state for line number state machine
  result.push_back(static_cast<uint8_t>(DW_LNS::set_file));
  constexpr uint8_t fileIndex = 0x01U;
  result.push_back(fileIndex);

  for (const auto &op : debugLine.sequences_) {
    toDwarf5FormatImpl(op, result);
  }

  // End of sequence
  result.push_back(static_cast<uint8_t>(DW_LNS::extended));
  /// @brief extended debug line opcodes
  enum class DW_LNE : uint8_t { end_sequence = 0x01U };
  result.push_back(0x01U); // length
  result.push_back(static_cast<uint8_t>(DW_LNE::end_sequence));

  // Fill in header_length
  uint32_t const headerLength = static_cast<uint32_t>(headerEnd - (headerLengthOffset + 4U));
  writeLE32(headerLength, result, headerLengthOffset);

  // Fill in unit_length
  uint32_t const unitLength = static_cast<uint32_t>(result.size() - (unitLengthOffset + 4U));
  writeLE32(unitLength, result, unitLengthOffset);
}

enum class DebugAbbrevCode : uint32_t {
  DW_TAG_null = 0U,
  DW_ABBREV_COMPILE_UNIT = 1U,
  DW_ABBREV_SUBPROGRAM = 2U,
};

enum class DW_CHILDREN : uint8_t {
  no = 0x00U, ///< DW_CHILDREN_no
  yes = 0x01U ///< DW_CHILDREN_yes
};

static void createDebugAbbrevSection(std::vector<uint8_t> &result) {
  // Abbreviation 1: DW_TAG_compile_unit
  encodeULEB128(static_cast<uint32_t>(DebugAbbrevCode::DW_ABBREV_COMPILE_UNIT), result);
  encodeULEB128(static_cast<uint32_t>(DW_TAG::compile_unit), result);
  result.push_back(static_cast<uint8_t>(DW_CHILDREN::yes)); // DW_CHILDREN_yes
  // Attribute list terminator
  result.push_back(0x00U);
  result.push_back(0x00U);

  // Abbreviation 2: DW_TAG_subprogram
  encodeULEB128(static_cast<uint32_t>(DebugAbbrevCode::DW_ABBREV_SUBPROGRAM), result);
  encodeULEB128(static_cast<uint32_t>(DW_TAG::subprogram), result);
  result.push_back(static_cast<uint8_t>(DW_CHILDREN::no));
  // Attributes for subprogram
  encodeULEB128(static_cast<uint32_t>(DW_AT::low_pc), result);
  encodeULEB128(static_cast<uint32_t>(DW_FORM::addr), result);
  encodeULEB128(static_cast<uint32_t>(DW_AT::high_pc), result);
  encodeULEB128(static_cast<uint32_t>(DW_FORM::addr), result);
  // Attribute list terminator
  result.push_back(0x00U);
  result.push_back(0x00U);

  // Abbreviation table terminator
  result.push_back(0x00U);
}

static void createDebugInfoSection(DebugInfo const &debugInfo, std::vector<uint8_t> &result) {
  // DWARF v5 Debug Info Header
  // See DWARF v5 spec, section 7.5.1.1

  // Reserve space for unit_length (4 bytes, will fill later)
  size_t const unitLengthOffset = result.size();
  result.resize(result.size() + 4U, 0U);

  // version (2 bytes)
  result.push_back(0x05U);
  result.push_back(0x00U);

  constexpr uint8_t DW_UT_compile = 0x01U;
  result.push_back(DW_UT_compile);

  // address_size (1 byte)
  constexpr uint8_t addressSize = static_cast<uint8_t>(8U);
  result.push_back(addressSize);

  // debug_abbrev_offset (4 bytes)
  pushLE32(0U, result);

  // Compile unit DIE using abbreviation code 1
  encodeULEB128(static_cast<uint32_t>(DebugAbbrevCode::DW_ABBREV_COMPILE_UNIT), result);

  for (const auto &function : debugInfo.functions_) {
    // Subprogram DIE using abbreviation code 2
    encodeULEB128(static_cast<uint32_t>(DebugAbbrevCode::DW_ABBREV_SUBPROGRAM), result);
    pushLE64(function.lowPC, result);
    pushLE64(function.highPC, result);
  }
  // Terminator for children of compile_unit
  result.push_back(0x00U);

  // Fill in unit_length
  uint32_t const unitLength = static_cast<uint32_t>(result.size() - (unitLengthOffset + 4U));
  result[unitLengthOffset + 0U] = static_cast<uint8_t>(unitLength >> 0U);
  result[unitLengthOffset + 1U] = static_cast<uint8_t>(unitLength >> 8U);
  result[unitLengthOffset + 2U] = static_cast<uint8_t>(unitLength >> 16U);
  result[unitLengthOffset + 3U] = static_cast<uint8_t>(unitLength >> 24U);
}

std::vector<uint8_t> Dwarf5Generator::toDwarfObject() const {
  std::vector<uint8_t> result{};
  // ELF Header

  // e_ident
  result.push_back(0x7FU); // EI_MAG0
  result.push_back(0x45U); // EI_MAG1
  result.push_back(0x4CU); // EI_MAG2
  result.push_back(0x46U); // EI_MAG3
  result.push_back(0x02U); // EI_CLASS (ELFCLASS64)
  result.push_back(0x01U); // EI_DATA (ELFDATA2LSB)
  result.push_back(0x01U); // EI_VERSION (EV_CURRENT)
  result.resize(16, 0U);   // EI_PAD (fill remaining bytes with 0)
  // e_type
  result.push_back(0x00U); // ET_NONE (no file type)
  result.push_back(0x00U);
  // e_machine
#if defined(JIT_TARGET_TRICORE)
  result.push_back(0x2CU); // EM_TRICORE (Tricore architecture)
  result.push_back(0x00U);
#elif defined(JIT_TARGET_AARCH64)
  result.push_back(0xB7U); // EM_AARCH64 (AArch64 architecture)
  result.push_back(0x00U);
#elif defined(JIT_TARGET_X86_64)
  result.push_back(0x3EU); // EM_X86_64 (x86-64 architecture)
  result.push_back(0x00U);
#endif
  // e_version
  pushLE32(0x01, result); // EV_CURRENT (current version)
  // e_entry
  pushLE64(0U, result);
  // e_phoff
  pushLE64(0U, result);
  // e_shoff
  const size_t eShoffOffset = result.size();
  pushLE64(0U, result);
  // e_flags
  pushLE32(0U, result);
  // e_ehsize
  result.push_back(0x40U); // ELF header size (64 bytes)
  result.push_back(0x00U);
  // e_phentsize
  result.push_back(0x00U); // no program headers
  result.push_back(0x00U);
  // e_phnum
  result.push_back(0x00U); // no program headers
  result.push_back(0x00U);
  // e_shentsize
  result.push_back(0x40U); // section header size (64 bytes)
  result.push_back(0x00U);
  // e_shnum
  result.push_back(0x05U); // empty, .shstrtab, .debug_line, .debug_abbrev, .debug_info
  result.push_back(0x00U);
  // e_shstrndx
  result.push_back(0x01U); // .shstrtab section index
  result.push_back(0x00U);

  // .shstrtab section
  const size_t shstrtabSectionOffset = result.size();
  result.push_back(0x00U);
  const size_t shstrtabStrOffset = result.size() - shstrtabSectionOffset;
  pushStr(".shstrtab", result);
  const size_t debugLineStrOffset = result.size() - shstrtabSectionOffset;
  pushStr(".debug_line", result);
  const size_t debugAbbrevStrOffset = result.size() - shstrtabSectionOffset;
  pushStr(".debug_abbrev", result);
  const size_t debugInfoStrOffset = result.size() - shstrtabSectionOffset;
  pushStr(".debug_info", result);
  const size_t shstrtabSectionLength = result.size() - shstrtabSectionOffset;

  // .debug_line section
  const size_t debugLineSectionOffset = result.size();
  createDebugLineSection(storage_.debugLine_, result);
  const size_t debugLineSectionLength = result.size() - debugLineSectionOffset;

  // // .debug_abbrev section
  const size_t debugAbbrevSectionOffset = result.size();
  createDebugAbbrevSection(result);
  const size_t debugAbbrevSectionLength = result.size() - debugAbbrevSectionOffset;

  // .debug_info section
  const size_t debugInfoSectionOffset = result.size();
  createDebugInfoSection(storage_.debugInfo_, result);
  const size_t debugInfoSectionLength = result.size() - debugInfoSectionOffset;

  // Section Header Table
  writeLE32(result.size(), result, eShoffOffset);
  // empty section header
  result.resize(result.size() + 0x40U, 0U);

  auto writeSectionHeader = [&result](size_t const nameOffset, uint32_t const type, size_t const sectionOffset, size_t const sectionLength) {
    // sh_name
    pushLE32(nameOffset, result);
    // sh_type
    pushLE32(type, result);
    // sh_flags
    pushLE64(0U, result);
    // sh_addr
    pushLE64(0U, result);
    // sh_offset
    pushLE64(sectionOffset, result);
    // sh_size
    pushLE64(sectionLength, result);
    // sh_link
    result.resize(result.size() + 4U, 0U);
    // sh_info
    result.resize(result.size() + 4U, 0U);
    // sh_addralign
    pushLE64(0x01U, result);
    // sh_entsize
    pushLE64(0U, result);
  };

  // .shstrtab section header
  constexpr uint32_t SHT_STRTAB = 0x03U;
  writeSectionHeader(shstrtabStrOffset, SHT_STRTAB, shstrtabSectionOffset, shstrtabSectionLength);

  // .debug_line section header
  constexpr uint32_t SHT_PROGBITS = 0x01U;
  writeSectionHeader(debugLineStrOffset, SHT_PROGBITS, debugLineSectionOffset, debugLineSectionLength);

  // .debug_abbrev section header
  writeSectionHeader(debugAbbrevStrOffset, SHT_PROGBITS, debugAbbrevSectionOffset, debugAbbrevSectionLength);

  // .debug_info section header
  writeSectionHeader(debugInfoStrOffset, SHT_PROGBITS, debugInfoSectionOffset, debugInfoSectionLength);

  return result;
}

void Dwarf5Generator::dump(std::ostream &os) const {
  dumpImpl(storage_.debugLine_, os);
}

std::vector<uint32_t> Dwarf5Generator::getInstructions() const {
  uint32_t address{0U};
  std::vector<uint32_t> output{};
  for (const DebugLineOpCode &op : storage_.debugLine_.sequences_) {
    switch (op.kind_) {
    case DebugLineOpCode::OpCodeKind::advance_pc:
      address += op.v_.advancePC_.offset_;
      break;
    case DebugLineOpCode::OpCodeKind::advance_line:
      break;
    case DebugLineOpCode::OpCodeKind::copy:
      output.push_back(address);
      break;
    default:
      UNREACHABLE(, "Unsupported DebugLineOpCode kind: " << static_cast<uint8_t>(op.kind_));
      break;
    }
  }
  return output;
}

void Dwarf5Generator::registerPendingDeferAction(StackElement const *stackElement, uint32_t const sourceOffset) {
  pendingDeferActions_[stackElement] = sourceOffset;
}

void Dwarf5Generator::startOp(StackElement const *stackElement) {
  assert(pendingDeferActions_.find(stackElement) != pendingDeferActions_.end() && "Stack element must have a pending defer action");
  startOp(pendingDeferActions_.at(stackElement));
  pendingDeferActions_.erase(stackElement);
}

void Dwarf5Generator::startOp(uint32_t const sourceOffset) {
  sourceOffsetStack_.push(sourceOffset);
}
void Dwarf5Generator::finishOp() {
  assert(!sourceOffsetStack_.empty());
  sourceOffsetStack_.pop();
}
void Dwarf5Generator::record(uint32_t const destinationOffset) {
  if (sourceOffsetStack_.empty()) {
    return;
  }
  addSourceDestinationMap(sourceOffsetStack_.top(), destinationOffset);
}

void Dwarf5Generator::addSourceDestinationMap(uint32_t const sourceOffset, uint32_t const destinationOffset) {
  if (destinationOffset != currentDestinationOffset_) {
    storage_.debugLine_.sequences_.push_back(createAdvancePC(destinationOffset - currentDestinationOffset_));
  }
  if (sourceOffset != currentSourceOffset_) {
    storage_.debugLine_.sequences_.push_back(createAdvanceLine(static_cast<int32_t>(sourceOffset - currentSourceOffset_)));
  }
  storage_.debugLine_.sequences_.push_back(createCopy());
  currentSourceOffset_ = sourceOffset;
  currentDestinationOffset_ = destinationOffset;
}

void Dwarf5Generator::startFunction(uint32_t const destinationOffset) {
  storage_.debugInfo_.functions_.emplace_back(DebugInfo::Function{.lowPC = destinationOffset, .highPC = 0U});
}

void Dwarf5Generator::finishFunction(uint32_t const destinationOffset) {
  assert(!storage_.debugInfo_.functions_.empty());
  storage_.debugInfo_.functions_.back().highPC = destinationOffset;
}

} // namespace extension
} // namespace vb

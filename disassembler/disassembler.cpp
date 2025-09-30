///
/// @file disassembler.cpp
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

#include <cassert>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <iostream>
#include <optional>
#include <ranges>
#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <vector>

#include "disassembler.hpp"
#include "disassembler/color.hpp"

#include "src/config.hpp"
#include "src/core/common/Span.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/common/MachineType.hpp"

#if VB_DISASSEMBLER_FOUND_CAPSTONE
#include <capstone/capstone.h>
#endif

namespace {
template <typename Type> inline void printNumeric(Type const value, std::stringstream &buffer) {
  buffer << value;
}
template <> inline void printNumeric(uint8_t const value, std::stringstream &buffer) {
  buffer << static_cast<uint32_t>(value);
}
} // namespace

namespace vb {
namespace disassembler {

class DisassemblerImpl final {
private:
  std::vector<uint32_t> const &instructionAddresses_;
  struct MultiStringOutput {
    bool hasTwoStrings;
    std::string first;
    std::string second;
  };
  static MultiStringOutput printBytes(uint8_t const *ptr, size_t count, bool isMachineCode = false);
  MultiStringOutput printMachineCode(uint8_t const *ptr, size_t count, size_t paddedSize, size_t baseAddress);

  template <typename Type> struct NumericTypeResult {
    std::string output;
    Type value;
  };
  template <typename Type>
  static NumericTypeResult<Type> consumeNumericType(uint8_t const *&stepPtr, std::string const &description, bool forward, bool printAsHex = false,
                                                    bool suppress = false);
  template <typename Type> struct DualNumericTypeResult {
    std::string output;
    Type value1;
    Type value2;
  };
  template <typename Type>
  static DualNumericTypeResult<Type> consumeDualNumericType(uint8_t const *&stepPtr, std::string const &description, bool forward,
                                                            bool printAsHex = false);
  template <typename Type> static Type peekNumericType(uint8_t const *stepPtr, bool forward);

  MultiStringOutput consumeBinary(uint8_t const *&stepPtr, std::string const &description, uint32_t size, uint32_t paddedAlignmentPow2 = 2,
                                  bool isMachineCode = false, size_t baseAddress = 0);
  static MultiStringOutput consumeString(uint8_t const *&stepPtr, std::string const &description, uint32_t size, uint32_t paddedAlignmentPow2 = 2);

public:
  explicit DisassemblerImpl(std::vector<uint32_t> const &instructionAddresses) noexcept : instructionAddresses_(instructionAddresses) {
    static_cast<void>(instructionAddresses_);
  }
  std::string disassemble(uint8_t const *const binaryData, size_t const binarySize);
  static std::string disassembleDebugMap(uint8_t const *const binaryData, size_t const binarySize);
};

DisassemblerImpl::MultiStringOutput DisassemblerImpl::printBytes(uint8_t const *ptr, size_t count, bool isMachineCode) {
  std::stringstream buffer;
  for (size_t i = 0; i < count; i++) {
    if (i > 0) {
      buffer << " ";
    }
    uint8_t const byte = readFromPtr<uint8_t>(ptr + i);
    buffer << std::hex << std::setw(2) << std::setfill('0') << static_cast<uint32_t>(byte);
  }

  std::string const rawBytesString = buffer.str();
  buffer.str("");

  constexpr size_t bytesColumnWidth = 29;
  if (rawBytesString.size() > bytesColumnWidth) {
    buffer << (isMachineCode ? TtyControl::Blue : TtyControl::Green);
    buffer << rawBytesString;
    buffer << TtyControl::Reset;
    std::string const second = buffer.str();
    buffer.str("");
    for (size_t i = 0; i < std::min(count, static_cast<size_t>(10U)); i++) {
      if (i > 0) {
        buffer << " ";
      }
      buffer << "vv";
    }
    return {true, buffer.str(), second};
  } else {
    buffer << (isMachineCode ? TtyControl::Blue : TtyControl::Green);
    buffer << std::setw(bytesColumnWidth) << std::setfill(' ') << rawBytesString;
    buffer << TtyControl::Reset;
    return {false, buffer.str(), std::string()};
  }
}

#if VB_DISASSEMBLER_FOUND_CAPSTONE

#if defined(JIT_TARGET_AARCH64)
constexpr const cs_arch ARCH = CS_ARCH_AARCH64;
constexpr const cs_mode MODE = CS_MODE_ARM;
#elif defined(JIT_TARGET_X86_64)
constexpr const cs_arch ARCH = CS_ARCH_X86;
constexpr const cs_mode MODE = CS_MODE_64;
#elif defined(JIT_TARGET_TRICORE)
constexpr const cs_arch ARCH = CS_ARCH_TRICORE;
constexpr const cs_mode MODE = CS_MODE_TRICORE_180;
#else
static_assert(false, "Backend not supported");
#endif

class CSDissembler final {
public:
  CSDissembler(cs_arch arch, cs_mode mode, std::vector<uint32_t> const &instructionAddresses) noexcept
      : instructionAddresses_(instructionAddresses), handle_(0U), err_(cs_open(arch, mode, &handle_)) {
  }

  CSDissembler(CSDissembler &) = delete;
  CSDissembler &operator=(const CSDissembler &) = delete;
  CSDissembler(CSDissembler &&other) = delete;
  CSDissembler &operator=(CSDissembler &&other) = delete;

  ~CSDissembler() noexcept {
    cs_close(&handle_);
  }

  Span<uint32_t const> split(uint64_t const baseAddress, size_t const size) noexcept {
    std::optional<size_t> beginIndex = std::nullopt;
    std::optional<size_t> endIndex = std::nullopt;
    size_t index = 0U;
    for (; index < instructionAddresses_.size(); index++) {
      if (instructionAddresses_[index] >= baseAddress) {
        beginIndex = index;
        break;
      }
    }
    for (; index < instructionAddresses_.size(); index++) {
      if (instructionAddresses_[index] >= (baseAddress + size)) {
        endIndex = index;
        break;
      }
    }
    if (beginIndex.has_value() && endIndex.has_value()) {
      return Span<uint32_t const>{&instructionAddresses_[beginIndex.value()], endIndex.value() - beginIndex.value()};
    } else if (beginIndex.has_value()) {
      return Span<uint32_t const>{&instructionAddresses_[beginIndex.value()], instructionAddresses_.size() - beginIndex.value()};
    } else {
      return Span<uint32_t const>{};
    }
  }

  std::vector<cs_insn> disasm(uint8_t const *ptr, size_t size, uint64_t baseAddress) {
    std::vector<cs_insn> result{};
    Span<uint32_t const> const instructionAddresses = split(baseAddress, size);
    size_t nextInstructionIndex = 0U;
    while (size > 0U) {
      cs_insn insn;
      std::memset(&insn, 0, sizeof(insn));
      bool succ = true;
      if (nextInstructionIndex < instructionAddresses.size()) {
        if (baseAddress == static_cast<uint64_t>(instructionAddresses[nextInstructionIndex])) {
          // find instruction
          succ = cs_disasm_iter(handle_, &ptr, &size, &baseAddress, &insn);
          if (!succ) {
            break;
          }
          nextInstructionIndex += 1U;
        } else {
          assert(baseAddress < static_cast<uint64_t>(instructionAddresses[nextInstructionIndex]));
          if (nextInstructionIndex == 0U) {
            // before all code related instructions
            succ = cs_disasm_iter(handle_, &ptr, &size, &baseAddress, &insn);
            if (!succ) {
              break;
            }
          } else {
            // raw bytes
            if (static_cast<uint64_t>(instructionAddresses[nextInstructionIndex]) - baseAddress >= 4U && size >= 4U) {
              insn.address = baseAddress;
              std::memcpy(insn.bytes, ptr, 4U);
              insn.size = 4U;
              uint32_t integer = 0;
              std::memcpy(&integer, insn.bytes, sizeof(integer));
              std::stringstream ss{};
              ss << "byte[" << std::hex << integer << "]";
              std::string mnemonic = std::move(ss).str();
              std::memcpy(insn.mnemonic, mnemonic.data(), mnemonic.size());
              ptr += 4U;
              size -= 4U;
              baseAddress += 4U;
            } else {
              insn.address = baseAddress;
              insn.bytes[0] = *ptr;
              insn.size = 1U;
              std::memcpy(insn.mnemonic, "byte", sizeof("byte"));
              ptr += 1U;
              size -= 1U;
              baseAddress += 1U;
            }
          }
        }
      } else {
        // finished all code related instructions
        succ = cs_disasm_iter(handle_, &ptr, &size, &baseAddress, &insn);
        if (!succ) {
          break;
        }
      }
      result.push_back(insn);
    }
    return result;
  }

  inline cs_err getError() const {
    return err_;
  }

  static const char *errToStr(cs_err err) {
    return cs_strerror(err);
  }

private:
  std::vector<uint32_t> const &instructionAddresses_;
  csh handle_;
  cs_err err_;
};

static std::string trimtabs(const std::string &str) {
  size_t const first = str.find_first_not_of('\t');
  if (std::string::npos == first) {
    return str;
  }
  size_t const last = str.find_last_not_of('\t');
  return str.substr(first, (last - first + 1));
}

DisassemblerImpl::MultiStringOutput DisassemblerImpl::printMachineCode(uint8_t const *ptr, size_t count, size_t paddedSize,
                                                                       size_t const baseAddress) {
  std::stringstream second;
  CSDissembler dis{ARCH, MODE, instructionAddresses_};

  second << TtyControl::Blue;
  if (dis.getError() != CS_ERR_OK) {
    return printBytes(ptr, paddedSize, true);
  }
  std::vector<cs_insn> const insn = dis.disasm(ptr, count, baseAddress);
  if (insn.empty()) {
    return printBytes(ptr, paddedSize, true);
  }
  second << TtyControl::Reset;

  struct InternalMultiStringOutput {
    uint32_t offset = 0;
    std::string first;
    std::string second;
  };
  std::vector<InternalMultiStringOutput> outputBuffer;
  uint16_t maxInstSize = 0;
  for (const cs_insn &index : insn) {
    InternalMultiStringOutput out;
    out.offset = static_cast<uint32_t>(index.address);
    if (index.size > maxInstSize) {
      maxInstSize = index.size;
    }

    std::stringstream instrbuf;
    instrbuf << "  ";
    instrbuf << index.mnemonic << "  ";
    instrbuf << trimtabs(index.op_str);
    out.second = instrbuf.str();

    std::stringstream hexbuf;
    for (size_t i = 0; i < index.size; i++) {
      if (i > 0) {
        hexbuf << " ";
      }
      uint8_t const byte = index.bytes[i];
      hexbuf << std::hex << std::setw(2) << std::setfill('0') << static_cast<uint32_t>(byte);
    }
    out.first = hexbuf.str();

    outputBuffer.push_back(out);
  }

  int const bytesColumnWidth = std::max(29, static_cast<int>(maxInstSize));
  for (uint32_t i = 0; i < outputBuffer.size(); i++) {
    auto elem = outputBuffer[i];

    std::stringstream offsetbuf;
    offsetbuf << std::hex << std::left << std::setw(8) << elem.offset << " ";

    std::stringstream temp;
    temp << std::setw(bytesColumnWidth) << elem.first;

    second << TtyControl::Dim << offsetbuf.str() << TtyControl::Reset << TtyControl::Blue << temp.str() << TtyControl::Reset << elem.second;
    if (i != outputBuffer.size() - 1) {
      second << "\n";
    }
  }

  std::stringstream first;
  first.str("");
  for (size_t i = 0; i < std::min(count, static_cast<size_t>(10U)); i++) {
    if (i > 0) {
      first << " ";
    }
    first << "vv";
  }
  return {true, first.str(), second.str()};
}
#else // VB_DISASSEMBLER_FOUND_CAPSTONE

DisassemblerImpl::MultiStringOutput DisassemblerImpl::printMachineCode(uint8_t const *ptr, size_t /*count*/, size_t paddedSize, size_t baseAddress) {
  static_cast<void>(baseAddress);
  return printBytes(ptr, paddedSize, true);
}

#endif // VB_DISASSEMBLER_FOUND_CAPSTONE

template <class Dest> static Dest readValue(uint8_t const **const ptr, bool const forward) noexcept {
  if (!forward) {
    *ptr = pSubI(*ptr, sizeof(Dest));
  }
  Dest val = readFromPtr<Dest>(*ptr);
  if (forward) {
    *ptr = pAddI(*ptr, sizeof(Dest));
  }
  return val;
}

template <typename Type>
DisassemblerImpl::NumericTypeResult<Type> DisassemblerImpl::consumeNumericType(uint8_t const *&stepPtr, std::string const &description, bool forward,
                                                                               bool printAsHex, bool suppress) {
  static_assert(std::is_integral<Type>::value || std::is_floating_point<Type>::value, "Type must be an integer or a float");

  std::stringstream buffer;
  Type const value = readValue<Type>(&stepPtr, forward);

  uint8_t const *const valueStart = forward ? stepPtr - sizeof(Type) : stepPtr;
  MultiStringOutput const bytesOutput = printBytes(valueStart, sizeof(Type));
  assert((bytesOutput.hasTwoStrings == false) && "Numeric cannot have two strings");
  buffer << bytesOutput.first << "  " << description;
  if (!suppress) {
    buffer << ": ";
    if (printAsHex) {
      buffer << std::hex;
    }
    printNumeric(value, buffer);
  }
  return {buffer.str(), value};
}
template <typename Type>
DisassemblerImpl::DualNumericTypeResult<Type> DisassemblerImpl::consumeDualNumericType(uint8_t const *&stepPtr, std::string const &description,
                                                                                       bool forward, bool printAsHex) {
  static_assert(std::is_integral<Type>::value || std::is_floating_point<Type>::value, "Type must be an integer or a float");

  std::stringstream buffer;
  Type const value1 = readValue<Type>(&stepPtr, forward);
  Type const value2 = readValue<Type>(&stepPtr, forward);

  uint8_t const *const valueStart = forward ? stepPtr - (2 * sizeof(Type)) : stepPtr;
  MultiStringOutput const bytesOutput = printBytes(valueStart, 2 * sizeof(Type));
  assert((bytesOutput.hasTwoStrings == false) && "Numeric cannot have two strings");
  buffer << bytesOutput.first << "  ";
  buffer << description << ": ";
  if (printAsHex) {
    buffer << std::hex;
  }
  printNumeric(value1, buffer);
  buffer << ", ";
  printNumeric(value2, buffer);
  return {buffer.str(), value1, value2};
}
template <typename Type> Type DisassemblerImpl::peekNumericType(uint8_t const *stepPtr, bool forward) {
  static_assert(std::is_integral<Type>::value || std::is_floating_point<Type>::value, "Type must be an integer or a float");
  return readValue<Type>(&stepPtr, forward);
}

DisassemblerImpl::MultiStringOutput DisassemblerImpl::consumeBinary(uint8_t const *&stepPtr, std::string const &description, uint32_t size,
                                                                    uint32_t paddedAlignmentPow2, bool isMachineCode, size_t baseAddress) {
  std::stringstream buffer;
  size_t const paddedSize = roundUpToPow2(size, paddedAlignmentPow2);
  size_t const alignedBaseAddress = (baseAddress >> paddedAlignmentPow2) << paddedAlignmentPow2;

  stepPtr = pSubI(stepPtr, paddedSize);
  uint8_t const *const binaryStart = stepPtr;
  MultiStringOutput const bytesOutput =
      isMachineCode ? printMachineCode(binaryStart, size, paddedSize, alignedBaseAddress) : printBytes(binaryStart, paddedSize);
  buffer << bytesOutput.first << "  ";
  buffer << description << " ";
  return {bytesOutput.hasTwoStrings, buffer.str(), bytesOutput.second};
}
DisassemblerImpl::MultiStringOutput DisassemblerImpl::consumeString(uint8_t const *&stepPtr, std::string const &description, uint32_t size,
                                                                    uint32_t paddedAlignmentPow2) {
  std::stringstream buffer;
  size_t const paddedSize = roundUpToPow2(size, paddedAlignmentPow2);

  stepPtr = pSubI(stepPtr, paddedSize);
  uint8_t const *const stringStart = stepPtr;
  MultiStringOutput const bytesOutput = printBytes(stringStart, paddedSize);
  buffer << bytesOutput.first << "  ";
  buffer << description << ": \"";
  buffer.write(vb::pCast<char const *>(stringStart), static_cast<std::streamsize>(size));
  buffer << "\"";

  return {bytesOutput.hasTwoStrings, buffer.str(), bytesOutput.second};
}

static void pushString(std::string const &str, std::vector<std::string> &outputBuffer, std::ptrdiff_t const offset = PTRDIFF_MAX) {
  bool const withOffset = offset != PTRDIFF_MAX;
  std::stringstream buffer;
  if (withOffset) {
    buffer << TtyControl::Dim;
    buffer << std::hex << std::left << std::setw(8) << static_cast<uint32_t>(offset) << " ";
    buffer << TtyControl::Reset;
  }
  buffer << str;
  outputBuffer.push_back(buffer.str());
}

std::string DisassemblerImpl::disassemble(uint8_t const *const binaryData, size_t const binarySize) {
  assert(binarySize != 0 && "Invalid binary");

  std::vector<std::string> outputBuffer;

  // Let's set the cursor to the end of the binary
  uint8_t const *stepPtr = pAddI(binaryData, binarySize);

  auto getCurrentOffset = [&stepPtr, &binaryData]() -> ptrdiff_t {
    return stepPtr - binaryData;
  };

  auto consumeGenericU32 = [&stepPtr, &outputBuffer, &getCurrentOffset](std::string const &description) -> uint32_t {
    NumericTypeResult<uint32_t> const res = consumeNumericType<uint32_t>(stepPtr, description, false, false, true);
    pushString(res.output, outputBuffer, getCurrentOffset());
    return res.value;
  };
  auto consumeU32 = [&stepPtr, &outputBuffer, &getCurrentOffset](std::string const &description) -> uint32_t {
    NumericTypeResult<uint32_t> const res = consumeNumericType<uint32_t>(stepPtr, description, false);
    pushString(res.output, outputBuffer, getCurrentOffset());
    return res.value;
  };
  auto consumeU8 = [&stepPtr, &outputBuffer, &getCurrentOffset](std::string const &description, bool printAsHex = false) -> uint8_t {
    NumericTypeResult<uint8_t> const res = consumeNumericType<uint8_t>(stepPtr, description, false, printAsHex);
    pushString(res.output, outputBuffer, getCurrentOffset());
    return res.value;
  };
  auto consumeU64 = [&stepPtr, &outputBuffer, &getCurrentOffset](std::string const &description) -> uint64_t {
    NumericTypeResult<uint64_t> const res = consumeNumericType<uint64_t>(stepPtr, description, false);
    pushString(res.output, outputBuffer, getCurrentOffset());
    return res.value;
  };
  auto consumeF32 = [&stepPtr, &outputBuffer, &getCurrentOffset](std::string const &description) -> float {
    NumericTypeResult<float> const res = consumeNumericType<float>(stepPtr, description, false);
    pushString(res.output, outputBuffer, getCurrentOffset());
    return res.value;
  };
  auto consumeF64 = [&stepPtr, &outputBuffer, &getCurrentOffset](std::string const &description) -> double {
    NumericTypeResult<double> const res = consumeNumericType<double>(stepPtr, description, false);
    pushString(res.output, outputBuffer, getCurrentOffset());
    return res.value;
  };
  auto consumeChar = [&stepPtr, &outputBuffer, &getCurrentOffset](std::string const &description) -> char {
    MultiStringOutput const res = consumeString(stepPtr, description, 1, 0);
    assert((res.hasTwoStrings == false) && "Char cannot have two strings");
    pushString(res.first, outputBuffer, getCurrentOffset());
    return readFromPtr<char>(stepPtr);
  };
  auto consumeBin = [this, &stepPtr, &outputBuffer, &getCurrentOffset](uint32_t size, std::string const &description, uint8_t paddingPow2 = 2,
                                                                       bool isMachineCode = false) {
    size_t const baseAddress = static_cast<size_t>(getCurrentOffset()) - size;
    MultiStringOutput const res = consumeBinary(stepPtr, description, size, paddingPow2, isMachineCode, baseAddress);
    if (res.hasTwoStrings) {
      pushString(res.second, outputBuffer);
    }
    pushString(res.first, outputBuffer, getCurrentOffset());
  };
  auto consumeStr = [&stepPtr, &outputBuffer, &getCurrentOffset](uint32_t size, std::string const &description, uint8_t paddingPow2 = 2) {
    MultiStringOutput const res = consumeString(stepPtr, description, size, paddingPow2);
    if (res.hasTwoStrings) {
      pushString(res.second, outputBuffer);
    }
    pushString(res.first, outputBuffer, getCurrentOffset());
  };
  auto printSectionInfo = [&outputBuffer](std::string sectionName, bool newline = true) {
    std::stringstream buffer;
    buffer << TtyControl::UnderLine;
    buffer << sectionName;
    buffer << TtyControl::Reset;
    outputBuffer.push_back(buffer.str());
    if (newline) {
      outputBuffer.push_back("");
    };
  };
  auto getFinalOutput = [&outputBuffer]() -> std::string {
    std::stringstream finalOutputStream;
    for (const auto &it : std::ranges::reverse_view(outputBuffer)) {
      finalOutputStream << it << "\n";
    }
    return finalOutputStream.str();
  };
  auto toHex = [](uint32_t value) -> std::string {
    std::stringstream buffer;
    buffer << std::hex << value;
    return buffer.str();
  };
  auto toDec = [](uint32_t value) -> std::string {
    std::stringstream buffer;
    buffer << value;
    return buffer.str();
  };

  //
  // Misc
  //
  consumeU32("Module binary size (excl. this value)"); // OPBVMET3
  consumeU32("Version of this binary module"); // OPBVER                                                                                  // OPBVER
  uint32_t const stacktraceEntry = peekNumericType<uint32_t>(stepPtr, false);                                                            //
  uint32_t const stacktraceRecordCount = stacktraceEntry & ~0x8000'0000U;                                                                //
  uint32_t const debugMode = stacktraceEntry & 0x8000'0000U;                                                                             //
  consumeGenericU32("Stacktrace records " + std::to_string(stacktraceRecordCount) + ", debugMode: " + std::to_string(debugMode));        // OPBVMET2
  uint32_t const landingPadOffset = peekNumericType<uint32_t>(stepPtr, false);                                                           //
  uint32_t const landingPadPos = static_cast<uint32_t>(getCurrentOffset()) - static_cast<uint32_t>(sizeof(uint32_t)) - landingPadOffset; //
  consumeU32("Offset from here for the landing pad function body (0xFFFF'FFFF = undefined, pos: " + toHex(landingPadPos) + ")");         // OPBVMET1
  consumeU32("Size of link data");                                                                                                       // OPBVMET0
  printSectionInfo("More Info");

  uint32_t const tableEntryFunctionCount = consumeU32("Number of function table entries");
  for (size_t i = 0; i < tableEntryFunctionCount; i++) {
    consumeU32("Wrapper function offset in binary module");
  }

  //
  // Table
  //
  uint32_t const numTableEntries = consumeU32("Number of table entries");                                                              // OPBVT2
  for (size_t i = 0; i < numTableEntries; i++) {                                                                                       //
    consumeU32("Function signature index (0xFFFF'FFFF = undefined)");                                                                  // OPBVT1
    uint32_t const functionOffset = peekNumericType<uint32_t>(stepPtr, false);                                                         //
    uint32_t const functionPos = static_cast<uint32_t>(getCurrentOffset()) - static_cast<uint32_t>(sizeof(uint32_t)) - functionOffset; //
    consumeU32("Offset from here for the function body (0xFFFF'FFFF = undefined, pos: " + toHex(functionPos) + ")");                   // OPBVT0
  };
  printSectionInfo("WebAssembly Table");

  //
  // Link Status of Imported Functions
  //
  uint32_t const numImportedFunctions = consumeU32("Total number of imported functions"); // OPBILS3
  uint32_t const linkStatusPadding = deltaToNextPow2(numImportedFunctions, 2U);           //
  consumeBin(linkStatusPadding, "Padding for link status table", 0);                      // OPBILS2
  for (size_t i = 0; i < numImportedFunctions; i++) {                                     //
    consumeU8("Link status of function " + toDec(static_cast<uint32_t>(i)));              // OPBILS1
  };
  printSectionInfo("WebAssembly Link Status of Imported Functions");

  //
  // Exported Functions
  //
  consumeU32("Section size (excl. this value)");                                    // OPBVEF13
  uint32_t const numExportedFunctions = consumeU32("Number of exported functions"); // OPBVEF12
  for (size_t i = 0; i < numExportedFunctions; i++) {                               //
    consumeU32("WebAssembly function index");                                       // OPBVEF9
    uint32_t const exportNameLength = consumeU32("Export name length");             // OPBVEF8
    consumeStr(exportNameLength, "Export name, padded to 4B");                      // OPBVEF7, OPBVEF6
    uint32_t const signatureLength = consumeU32("Function signature length");       // OPBVEF5
    consumeStr(signatureLength, "Function signature, padded to 4B");                // OPBVEF4, OPBVEF3
    uint32_t const callWrapperSize = consumeU32("Function wrapper size");           // OPBVEF2
    consumeBin(callWrapperSize, "Function wrapper, translates C++ ABI to Wasm ABI, padded to 4B", 2,
               true); // OPBVEF1, OPBVEF0
  }
  printSectionInfo("Exported Functions");

  //
  // Exported Globals
  //
  consumeU32("Section size (excl. this value)");                                      // OPBVEG8
  uint32_t const numExportedGlobals = consumeU32("Number of exported globals");       // OPBVEG7
  for (size_t i = 0; i < numExportedGlobals; i++) {                                   //
    uint32_t const exportNameLength = consumeU32("Export name length");               // OPBVEG6
    consumeStr(exportNameLength, "Export name, padded to 4B");                        // OPBVEG5, OPBVEG4
    consumeBin(2, "Padding to align to 4B", 0);                                       // OPBVEG3
    char const type = consumeChar("Type of this global, i=I32, I=I64, f=F32, F=F64"); // OPBVEG2
    uint8_t const isMutable = consumeU8("Whether this global is mutable");            // OPBVEG1
    if (isMutable != 0) {                                                             //
      consumeU32("Offset at which this data will be placed in the link data");        // OPBVEG0A
    } else {                                                                          // OPBVEG0B
      std::string const desc = "Constant value of this global";
      switch (type) {
      case 'i':
        consumeU32(desc);
        break;
      case 'I':
        consumeU64(desc);
        break;
      case 'f':
        consumeF32(desc);
        break;
      case 'F':
        consumeF64(desc);
        break;
      default:
        throw std::runtime_error("Unknown global type");
      }
    }
  }
  printSectionInfo("Exported Globals");

  //
  // Linear Memory
  //
  consumeU32("Initial linear memory size in multiples of 64kB (0xFFFF'FFFF = no memory)"); // OPBVMEM0
  printSectionInfo("Linear Memory");

  //
  // Dynamically Imported Functions
  //
  consumeU32("Section size (excl. this value)");                                                   // OPBVIF11
  uint32_t const numDynImportedFunctions = consumeU32("Number of dynamically imported functions"); // OPBVIF10
  for (size_t i = 0; i < numDynImportedFunctions; i++) {                                           //
    uint32_t const moduleNameLength = consumeU32("Module name length");                            // OPBVIF9
    consumeStr(moduleNameLength, "Module name, padded to 4B");                                     // OPBVIF8, OPBVIF7
    uint32_t const functionImportNameLength = consumeU32("Function name length");                  // OPBVIF6
    consumeStr(functionImportNameLength, "Function name, padded to 4B");                           // OPBVIF5, OPBVIF4
    uint32_t const signatureLength = consumeU32("Function signature length");                      // OPBVIF3
    consumeStr(signatureLength, "Function signature, padded to 4B");                               // OPBVIF2, OPBVIF1
    consumeU32("Offset at which this data will be placed in the link data");                       // OPBVIF0
  }
  printSectionInfo("Dynamically Imported Functions");

  //
  // Mutable Non-Exported Globals
  //
  consumeU32("Section size (excl. this value)");                                                 // OPBVNG5
  uint32_t const numMutableGlobals = consumeU32("Number of mutable globals");                    // OPBVNG4
  for (size_t i = 0; i < numMutableGlobals; i++) {                                               //
    consumeBin(3, "Padding to align to 4B", 0);                                                  // OPBVNG3
    uint8_t const type = consumeU8("Type of this global, 7F=I32, 7E=I64, 7D=F32, 7C=F64", true); // OPBVNG2
    consumeU32("Offset at which this data will be placed in the link data");                     // OPBVNG1
    std::string const desc = "Initial value of this global";                                     // OPBVNG0
    switch (bit_cast<MachineType>(type)) {
    case MachineType::I32:
      consumeU32(desc);
      break;
    case MachineType::I64:
      consumeU64(desc);
      break;
    case MachineType::F32:
      consumeF32(desc);
      break;
    case MachineType::F64:
      consumeF64(desc);
      break;
    default:
      throw std::runtime_error("Unknown global type");
    }
  }
  printSectionInfo("Mutable Non-Exported Globals");

  //
  // Start Function
  //
  uint32_t const startFunctionSectionSize = consumeU32("Section size (excl. this value, 0 means no start function)"); // OPBVSF6
  if (startFunctionSectionSize > 0) {
    uint32_t const startFunctionSignatureLength = consumeU32("Start function signature length"); // OPBVSF5
    consumeStr(startFunctionSignatureLength, "Start function signature, padded to 4B");          // OPBVSF4, OPBVSF3
    uint32_t const startFunctionCallWrapperSize = consumeU32("Start function call wrapper that translates the C++ ABI to the Wasm ABI"); // OPBVSF2
    consumeBin(startFunctionCallWrapperSize, "Start function call wrapper, padded to 4B", 2, true); // OPBVSF1, OPBVSF0
  }
  printSectionInfo("Start Function");

  //
  // Function Names
  //
  consumeU32("Section size (excl. this value)");                            // OPBFN5
  uint32_t const numFunctionNames = consumeU32("Number of function names"); // OPBFN4
  for (size_t i = 0; i < numFunctionNames; i++) {                           //
    consumeU32("WebAssembly function index");                               // OPBFN3
    uint32_t const functionNameLength = consumeU32("Function name length"); // OPBFN2
    consumeStr(functionNameLength, "Function name");                        // OPBFN1, OPBFN0
  }
  printSectionInfo("Function Names");

  //
  // Initial Linear Memory Data
  //
  uint32_t const numDataSegments = consumeU32("Number of data segments"); // OPBVLM4
  for (size_t i = 0; i < numDataSegments; i++) {                          //
    consumeU32("Data segment offset");                                    // OPBVLM3
    uint32_t const dataSegmentSize = consumeU32("Data segment size");     // OPBVLM2
    consumeBin(dataSegmentSize, "Data segment values, padded to 4B");     // OPBVLM1, OPBVLM40
  }
  printSectionInfo("Initial Linear Memory Data");

  //
  // Wasm Function Bodies
  //
  while (getCurrentOffset() > 0) {
    uint32_t const functionBodySize = consumeU32("Size of the function body");       // OPBVF2
    consumeBin(functionBodySize, "Function or wrapper body, padded to 4B", 2, true); // OPBVF0, OPBVF1
  }
  printSectionInfo("WebAssembly Function Bodies", false);

  return getFinalOutput();
}

std::string DisassemblerImpl::disassembleDebugMap(uint8_t const *const binaryData, size_t const binarySize) {
  assert(binarySize != 0 && "Invalid binary");
  static_cast<void>(binarySize);

  std::vector<std::string> outputBuffer;

  // Let's set the cursor to the start of the binary
  uint8_t const *stepPtr = binaryData;

  auto getCurrentOffset = [&stepPtr, &binaryData]() -> ptrdiff_t {
    return stepPtr - binaryData;
  };

  auto consumeU32 = [&stepPtr, &outputBuffer, &getCurrentOffset](std::string const &description) -> uint32_t {
    auto const currentOffset = getCurrentOffset();
    NumericTypeResult<uint32_t> const res = consumeNumericType<uint32_t>(stepPtr, description, true);
    pushString(res.output, outputBuffer, currentOffset);
    return res.value;
  };

  struct DualU32 {
    uint32_t value1 = 0;
    uint32_t value2 = 0;
  };
  auto consumeDualU32 = [&stepPtr, &outputBuffer, &getCurrentOffset](std::string const &description) -> DualU32 {
    auto const currentOffset = getCurrentOffset();
    DualNumericTypeResult<uint32_t> const res = consumeDualNumericType<uint32_t>(stepPtr, description, true);
    pushString(res.output, outputBuffer, currentOffset);
    return {res.value1, res.value2};
  };
  auto getFinalOutput = [&outputBuffer]() -> std::string {
    std::stringstream finalOutputStream;
    // Add newlines
    size_t const numBufferedLines = outputBuffer.size();
    for (size_t i = 0; i < numBufferedLines; i++) {
      finalOutputStream << outputBuffer[i] << "\n";
    }
    return finalOutputStream.str();
  };

  consumeU32("Version of debug map");
  consumeU32("Offset of lastFramePtr (neg offset from linMem)");
  consumeU32("Offset of actualLinMemSize (neg offset from linMem)");
  consumeU32("Offset of linkDataStart (neg offset from linMem)");

  consumeU32("Offset of genericTrapHandler (offset from jit code)");

  uint32_t const numNonImportedGlobals = consumeU32("Number of non-imported mutable globals");
  for (size_t i = 0; i < numNonImportedGlobals; i++) {
    consumeU32("Wasm global index");
    consumeU32("Offset of global in linkData " + std::to_string(i));
  }
  uint32_t const numNonImportedFunctions = consumeU32("Number of non-imported functions");
  for (size_t i = 0; i < numNonImportedFunctions; i++) {
    consumeU32("Wasm function index");
    uint32_t const numLocals = consumeU32("Number of locals for this function");

    for (size_t j = 0; j < numLocals; j++) {
      consumeU32("Offset in stack frame of local " + std::to_string(j));
    }

    uint32_t const numMachineCodeEntries = consumeU32("Number of machine code entries");
    for (size_t k = 0; k < numMachineCodeEntries; k++) {
      consumeDualU32("In, out offsets");
    }
  }
  return getFinalOutput();
}

} // namespace disassembler

std::string disassembler::disassemble(uint8_t const *const binaryData, size_t const binarySize, std::vector<uint32_t> const &instructionAddresses) {
  return DisassemblerImpl{instructionAddresses}.disassemble(binaryData, binarySize);
}
std::string disassembler::disassembleDebugMap(uint8_t const *const binaryData, size_t const binarySize) {
  return DisassemblerImpl::disassembleDebugMap(binaryData, binarySize);
}

std::string disassembler::getConfiguration() {
  std::stringstream ss;
#ifdef JIT_TARGET_X86_64
  ss << "BACKEND=X86_64 ";
#endif
#ifdef JIT_TARGET_AARCH64
  ss << "BACKEND=AARCH64 ";
#endif
#ifdef JIT_TARGET_TRICORE
  ss << "BACKEND=TRICORE ";
#endif
  ss << "INTERRUPTION_REQUEST=" << INTERRUPTION_REQUEST << " ";
  ss << "ACTIVE_STACK_OVERFLOW_CHECK=" << ACTIVE_STACK_OVERFLOW_CHECK << " ";
  ss << "LINEAR_MEMORY_BOUNDS_CHECKS=" << LINEAR_MEMORY_BOUNDS_CHECKS << " ";
#ifdef VB_POSIX
  ss << "VB_POSIX=" << 1 << " ";
#else
  ss << "VB_POSIX=" << 0 << " ";
#endif
#ifdef VB_WIN32
  ss << "VB_WIN32=" << 1 << " ";
#else
  ss << "VB_WIN32=" << 0 << " ";
#endif
#ifdef __APPLE__
  ss << "APPLE=" << 1 << " ";
#else
  ss << "APPLE=" << 0 << " ";
#endif
  return ss.str();
}

} // namespace vb

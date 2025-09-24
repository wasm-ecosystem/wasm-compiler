///
/// @file Frontend.hpp
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
#ifndef FRONTEND_HPP
#define FRONTEND_HPP

#include <cstdint>

#include "BytecodeReader.hpp"

#include "src/core/common/FunctionRef.hpp"
#include "src/core/common/NativeSymbol.hpp"
#include "src/core/common/Span.hpp"
#include "src/core/compiler/common/Common.hpp"
#include "src/core/compiler/common/MachineType.hpp"
#include "src/core/compiler/common/ModuleInfo.hpp"
#include "src/core/compiler/common/Stack.hpp"
#include "src/core/compiler/frontend/SectionType.hpp"
#include "src/core/compiler/frontend/ValidationStack.hpp"

namespace vb {
// coverity[autosar_cpp14_m3_2_3_violation]
class Compiler;

///
/// @brief Frontend of the compiler
///
/// This class parses and validates the WebAssembly code, populates the compiler stack and then serializes most of the
/// output binary
///
class Frontend final {
public:
  ///
  /// @brief Construct a Frontend instance
  ///
  /// @param bytecode Reference to the Span onto the bytecode that should be parsed and compiled
  /// @param symbolList Span onto the linkable NativeSymbols for resolving imported symbols
  /// @param moduleInfo Reference to the ModuleInfo
  /// @param stack Reference to the compiler stack
  /// @param memory Reference to the compiler memory
  /// @param common Reference to the common instance
  /// @param compiler Reference to the compiler
  /// @param validationStack Reference to the wasm validation stack
  Frontend(Span<uint8_t const> const &bytecode, Span<NativeSymbol const> const &symbolList, ModuleInfo &moduleInfo, Stack &stack, MemWriter &memory,
           Common &common, Compiler &compiler, ValidationStack &validationStack) VB_NOEXCEPT;

  ///
  /// @brief Start the compilation of the WebAssembly module
  ///
  void startCompilation(bool const forceHighRegisterPressureForTesting = false);

private:
  ///
  /// @brief Validate the magic number identifier of the WebAssembly bytecode
  /// @throws ValidationException Wrong Wasm magic number
  void validateMagicNumber();

  ///
  /// @brief Validate whether the version of the WebAssembly module is supported by this compiler
  /// @throws ValidationException Wasm version not supported
  void validateVersion();

  ///
  /// @brief Parse a custom section in the WebAssembly module
  ///
  /// @param sectionEnd Pointer to expected end of this custom section
  /// @param preNameSectionAction Lambda that should be executed before parsing the section if this custom section
  /// represents the name section
  /// @throws ValidationException If the section is invalid
  /// @throws std::range_error If not enough memory is available
  void parseCustomSection(uint8_t const *const sectionEnd, FunctionRef<void()> const &preNameSectionAction);

  ///
  /// @brief Parse the WebAssembly type section
  ///
  /// @throws ValidationException If the section is invalid
  /// @throws ImplementationLimitationException If there are more types or params than supported
  /// @throws std::range_error If not enough memory is available
  void parseTypeSection();

  ///
  /// @brief Parse the WebAssembly import section
  ///
  /// @throws ValidationException If the section is invalid
  /// @throws ImplementationLimitationException If there are more imports than supported or the strings are too long or
  /// malformed
  /// @throws std::range_error If not enough memory is available
  /// @throws LinkingException If the requested import is not available provided to the compiler
  /// @throws FeatureNotSupportedException Imported tables, memories and globals currently not supported
  void parseImportSection();

  ///
  /// @brief Parse the WebAssembly function section
  ///
  void parseFunctionSection();

  ///
  /// @brief Parse the WebAssembly table section
  ///
  void parseTableSection();

  ///
  /// @brief Parse the WebAssembly memory section
  ///
  void parseMemorySection();

  ///
  /// @brief Parse the WebAssembly global section
  ///
  void parseGlobalSection();

  ///
  /// @brief Parse the WebAssembly export section
  ///
  void parseExportSection();

  ///
  /// @brief Parse the WebAssembly start section
  ///
  void parseStartSection();

  ///
  /// @brief Parse the WebAssembly element section
  ///
  void parseElementSection();

  ///
  /// @brief Parse the WebAssembly code section
  ///
  void parseCodeSection();

  ///
  /// @brief Parse the WebAssembly data count section
  ///
  void parseDataCountSection();

  ///
  /// @brief Parse the WebAssembly data section
  ///
  void parseDataSection();

  ///
  /// @brief Actions that should be executed after parsing each section (or if a section is not present and is thus
  /// skipped)
  ///
  /// @param sectionType Section type that has just been skipped or parsed
  /// @throws ValidationException If section order is invalid
  /// @throws std::range_error If not enough memory is available
  void postSectionAction(SectionType const sectionType);

  ///
  /// @brief Serialize the start function section in the output binary
  ///
  void serializeStartFunctionSection();

  ///
  /// @brief Serialize the WebAssembly globals section in the output binary
  ///
  void serializeWasmGlobalsBinarySection();

  ///
  /// @brief Serialize the dynamically imported functions section in the output binary
  ///
  void serializeDynamicFunctionImportBinarySection();

  ///
  /// @brief Serialize the memory section in the output binary
  ///
  void serializeMemoryBinarySection();

  ///
  /// @brief Serialize the exported globals section in the output binary
  ///
  void serializeExportedGlobalsBinarySection();

  ///
  /// @brief Serialize the exported functions section in the output binary
  ///
  void serializeExportedFunctionBinarySection();

  ///
  /// @brief Serialize the link status section in the output binary
  ///
  void serializeLinkStatusSection();

  ///
  /// @brief Serialize the table section in the output binary
  ///
  void serializeTableBinarySection();

  /// @brief Serialize the table entry functions section in the output binary. It sorts an array of offset to BinaryModule end of all table C++ to
  /// Wasm wrapper functions
  void serializeTableEntryFunctionWrapperSection();

  ///
  /// @brief Serialize the module metadata section in the output binary
  ///
  void serializeModuleMetadataBinarySection();

  ///
  /// @brief Set the current stack frame (BLOCK, LOOP, IFBLOCK or the function frame) unreachable
  ///
  void setCurrentFrameFormallyUnreachable() VB_NOEXCEPT;
  ///
  /// @brief Pop all element of current block which is unreachable
  ///
  void cleanCurrentBlockOnUnreachable() VB_NOEXCEPT;
  ///
  /// @brief Check whether the current stack frame (BLOCK, LOOP, IFBLOCK or the function frame) is unreachable
  ///
  /// @return bool Whether the current stack frame is unreachable
  bool currentFrameIsUnreachable() const VB_NOEXCEPT;

  ///
  /// @brief Find the targeted frame/block/function for a given branch depth
  ///
  /// @param branchDepth Branch depth of a branch instruction
  /// @return Stack::iterator Iterator to the structural StackElement on the stack that is targeted, returns nullptr if the
  /// function frame is targeted
  /// @throws ValidationException if the branchDepth is invalid
  Stack::iterator findTargetBlock(uint32_t const branchDepth) const;

  ///
  /// @brief Reduce potentially forwarded type indices to the actual underlying type index
  ///
  /// @param typeIndex Input type index that is of potentially forwarded type
  /// @return uint32_t Unique actual underlying type index
  /// @throws ValidationException If the function type is out of bounds
  uint32_t reduceTypeIndex(uint32_t const typeIndex) const;

  ///
  /// @brief Write the preamble of the debug map
  ///
  /// @throws std::range_error If not enough memory is available
  void writeDebugMapPreamble();

  ///
  /// @brief Write the function info to the debug map
  ///
  /// @param fncIndex Function index
  /// @return uint32_t Offset in the debug map where the machine code length should be inserted as uint32_t
  /// @throws std::range_error If not enough memory is available
  uint32_t writeDebugMapFunctionInfo(uint32_t const fncIndex);

  ///
  /// @brief Write an entry for a produced instruction to the debug map
  ///
  /// @throws std::range_error If not enough memory is available
  void writeDebugMapInstructionRecordIfNeeded();

  ///
  /// @brief Write an entry for a produced instruction to the debug map
  ///
  /// @param debugMapRef Offset in the debug map where the machine code length should be inserted as uint32_t
  /// @throws std::range_error If not enough memory is available
  void patchDebugMapRef(uint32_t const debugMapRef) const VB_NOEXCEPT;

  ///
  /// @brief Write a binary blob to the output binary, pad the size (at the end) to 4 bytes and put a 4 byte size field
  /// (uint32) after
  ///
  /// @param lambda Implementation to write the binary blob to the output
  void writePaddedBinaryBlob(FunctionRef<void()> const &lambda);
  ///
  /// @brief Since validation is done in independent logic. Simplify make up params for block here
  ///
  /// @param sigIndex Signature index of block/ifBlock/loop/frame
  inline void pushDummyParamsOnUnreachable(uint32_t const sigIndex) {
    // coverity[autosar_cpp14_a5_1_9_violation]
    moduleInfo_.iterateParamsForSignature(sigIndex, FunctionRef<void(MachineType)>([this](MachineType const machineType) {
                                            static_cast<void>(stack_.push(StackElement::dummyConst(machineType)));
                                          }));
  }
  ///
  /// @brief Since validation is done in independent logic. Simplify make up results for block here
  ///
  /// @param sigIndex Signature index of block/ifBlock/loop/frame
  inline void pushDummyResultOnUnreachable(uint32_t const sigIndex) {
    // coverity[autosar_cpp14_a5_1_9_violation]
    moduleInfo_.iterateResultsForSignature(sigIndex, FunctionRef<void(MachineType)>([this](MachineType const machineType) {
                                             static_cast<void>(common_.pushOperandsToStack(StackElement::dummyConst(machineType)));
                                           }));
  }
  ///
  /// @brief Get the sigIndex for block with `blockType ::= valtype?`
  ///
  /// @param wasmType valtype for the block
  /// @return the sigIndex of the custom signature added to the types()
  ///
  uint32_t getSigIndexForBlock(WasmType const wasmType) const VB_NOEXCEPT;

  /// @brief Pop a block and push the return values onto the stack
  /// @param blockIt Iterator to the block to be popped
  void popBlockAndPushReturnValues(Stack::iterator const blockIt) VB_NOEXCEPT;

  BytecodeReader br_;                          ///< Bytecode reader
  Span<NativeSymbol const> const &symbolList_; ///< NativeSymbols that can be imported
  ModuleInfo &moduleInfo_;                     ///< Reference to the ModuleInfo
  Stack &stack_;                               ///< Reference to the stack
  MemWriter &memory_;                          ///< Reference to the compiler memory
  Common &common_;                             ///< Reference to the common instance
  Compiler &compiler_;                         ///< Reference to the compiler

  ValidationStack &validationStack_; ///< Reference to the validation stack
};
} // namespace vb

#endif /* FRONTEND_H */

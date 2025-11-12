///
/// @file Frontend.cpp
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
#include <cstddef>
#include <cstdint>
#include <limits>

#include "WasmImportExportType.hpp"

#include "src/config.hpp"
#include "src/core/common/BinaryModule.hpp"
#include "src/core/common/FunctionRef.hpp"
#include "src/core/common/ILogger.hpp"
#include "src/core/common/NativeSymbol.hpp"
#include "src/core/common/SignatureType.hpp"
#include "src/core/common/Span.hpp"
#include "src/core/common/TrapCode.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/WasmType.hpp"
#include "src/core/common/basedataoffsets.hpp"
#include "src/core/common/implementationlimits.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/Compiler.hpp"
#include "src/core/compiler/backend/PlatformAdapter.hpp"
#include "src/core/compiler/backend/RegAdapter.hpp"
#include "src/core/compiler/backend/aarch64/aarch64_backend.hpp"
#include "src/core/compiler/backend/aarch64/aarch64_cc.hpp"
#include "src/core/compiler/backend/aarch64/aarch64_encoding.hpp"
#include "src/core/compiler/backend/tricore/tricore_backend.hpp"
#include "src/core/compiler/backend/tricore/tricore_cc.hpp"
#include "src/core/compiler/backend/tricore/tricore_encoding.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_backend.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_cc.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_encoding.hpp"
#include "src/core/compiler/common/BranchCondition.hpp"
#include "src/core/compiler/common/BuiltinFunction.hpp"
#include "src/core/compiler/common/Common.hpp"
#include "src/core/compiler/common/ListIterator.hpp"
#include "src/core/compiler/common/MachineType.hpp"
#include "src/core/compiler/common/MemWriter.hpp"
#include "src/core/compiler/common/ModuleInfo.hpp"
#include "src/core/compiler/common/OPCode.hpp"
#include "src/core/compiler/common/Stack.hpp"
#include "src/core/compiler/common/StackElement.hpp"
#include "src/core/compiler/common/StackType.hpp"
#include "src/core/compiler/common/VariableStorage.hpp"
#include "src/core/compiler/common/util.hpp"
#include "src/core/compiler/frontend/BytecodeReader.hpp"
#include "src/core/compiler/frontend/Frontend.hpp"
#include "src/core/compiler/frontend/SectionType.hpp"
#include "src/core/compiler/frontend/ValidateElement.hpp"
#include "src/core/compiler/frontend/ValidationStack.hpp"

namespace vb {

///
/// @brief Validate a UTF-8 sequence with the given length
///
/// This is used for validating strings (export/import names etc.) of a Wasm module
///
/// @param start Pointer to the start of the string
/// @param length Length of the string to validate
/// @return bool Whether the given string and length represent valid UTF-8 sequence
static bool internal_validateUTF8(char const *const start, size_t const length) VB_NOEXCEPT {
  char const *step{start};
  uint32_t pendingContinuations{0U};

  uint32_t totalBytes{0U};
  uint32_t codepoint{0U};
  while (step < pAddI(start, length)) {
    uint8_t const currentByte{bit_cast<uint8_t>(*step)};
    step = pAddI(step, 1);
    // Left shift byte so it will be in the MSB of an unsigned long long so the clzll intrinsic can be used to count the
    // leading ones of the byte
    uint64_t const adjustedByte{static_cast<uint64_t>(currentByte) << ((sizeof(uint64_t) - 1U) * 8U)};
    uint32_t const leadingOnes{static_cast<uint32_t>(clzll(~adjustedByte))};

    if (pendingContinuations == 0U) {
      totalBytes = (leadingOnes == 0U) ? 1U : leadingOnes;
      pendingContinuations = leadingOnes;
      codepoint = 0U;

      if ((leadingOnes == 1U) || (leadingOnes > 4U)) {
        return false;
      }
      if (leadingOnes == 0U) {
        continue;
      }
    } else if (leadingOnes != 1U) {
      return false;
    } else {
      static_cast<void>(0);
    }
    pendingContinuations--;

    codepoint |= (static_cast<uint32_t>(currentByte) & static_cast<uint32_t>(0xFF_U32 >> (leadingOnes + 1U))) << (pendingContinuations * 6U);

    if (pendingContinuations == 0U) {
      // Codepoint finished, check if valid
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto firstCodepoint = make_array(0x00_U32, 0x80_U32, 0x800_U32, 0x10000_U32);
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto lastCodepoint = make_array(0x7F_U32, 0x7FFF_U32, 0xFFFF_U32, 0x10FFFF_U32);
      if ((codepoint < firstCodepoint[totalBytes - 1U]) || (codepoint > lastCodepoint[totalBytes - 1U])) {
        return false;
      }
      if ((codepoint >= 0xD800U) && (codepoint <= 0xDFFFU)) {
        return false; // Reserved for UTF-16 surrogate halves
      }
    }
  }

  return pendingContinuations == 0U;
}

///
/// @brief Validate a UTF-8 sequence with the given length and throw an exception if it isn't valid
///
/// This is used for validating strings (export/import names etc.) of a Wasm module
///
/// @param start Pointer to the start of the string
/// @param length Length of the string to validate
/// @throws ValidationException if the UTF-8 sequence is malformed
static void validateUTF8(char const *const start, size_t const length) {
  if (!internal_validateUTF8(start, length)) {
    throw ValidationException(ErrorCode::Malformed_UTF_8_sequence);
  }
}

void Frontend::writeDebugMapPreamble() {
  compiler_.debugMap_.write<uint32_t>(2_U32);                                                     // Write debug map version
  compiler_.debugMap_.write<uint32_t>(static_cast<uint32_t>(Basedata::FromEnd::lastFrameRefPtr)); // Write offset of debug ptr from linear memory
  compiler_.debugMap_.write<uint32_t>(
      static_cast<uint32_t>(Basedata::FromEnd::actualLinMemByteSize)); // Write offset of actual linear memory size from linear memory
  uint32_t const basedataLength{Basedata::length(moduleInfo_.linkDataLength, compiler_.getStacktraceRecordCount())}; //
  compiler_.debugMap_.write<uint32_t>(
      static_cast<uint32_t>(basedataLength - Basedata::FromStart::linkData)); // Write offset of start of link data from linear memory

  compiler_.debugMap_.write<uint32_t>(
      static_cast<uint32_t>(moduleInfo_.helperFunctionBinaryPositions.genericTrapHandler)); // Write offset of genericTrapHandler from code

  uint32_t numMutableGlobals{0U};
  uint32_t const globalStartOffset{compiler_.debugMap_.size()};
  compiler_.debugMap_.write<uint32_t>(moduleInfo_.numNonImportedGlobals); // Write number of non-imported globals
  for (uint32_t i{0U}; i < moduleInfo_.numNonImportedGlobals; i++) {
    ModuleInfo::GlobalDef const &globalDef{moduleInfo_.globals[i]};
    if (globalDef.isMutable) {
      compiler_.debugMap_.write<uint32_t>(i);                        // Write index of mutable global
      compiler_.debugMap_.write<uint32_t>(globalDef.linkDataOffset); // Write offset in link data of global
      numMutableGlobals++;
    }
  }
  uint8_t *const patchPtr{compiler_.debugMap_.posToPtr(globalStartOffset)};
  writeToPtr<uint32_t>(patchPtr, numMutableGlobals);

  uint32_t const numNonImportedFunctions{moduleInfo_.numTotalFunctions - moduleInfo_.numImportedFunctions};
  compiler_.debugMap_.write<uint32_t>(numNonImportedFunctions); // Write number of non-imported functions for which we have debug symbols
}

uint32_t Frontend::writeDebugMapFunctionInfo(uint32_t const fncIndex) {
  uint32_t debugMapRef{0U}; // Position where we should patch later
  compiler_.debugMap_.write<uint32_t>(fncIndex);
  compiler_.debugMap_.write<uint32_t>(moduleInfo_.fnc.numLocals);
  for (uint32_t i{0U}; i < moduleInfo_.fnc.numLocals; i++) {
    ModuleInfo::LocalDef const localDef{moduleInfo_.localDefs[i]};
    assert(localDef.currentStorageType == StorageType::STACKMEMORY && "Local not allocated on stack");
    compiler_.debugMap_.write<uint32_t>(localDef.stackFramePosition);
  }
  debugMapRef = compiler_.debugMap_.size();                          // Store reference for later patching of machine code length
  compiler_.debugMap_.step(static_cast<uint32_t>(sizeof(uint32_t))); // Reserve space for machine code length (will be patched later)
  return debugMapRef;
}

void Frontend::writeDebugMapInstructionRecordIfNeeded() {
  // Check if new machine code was produced in the output
  if (moduleInfo_.outputSizeBeforeLastParsedInstruction >= compiler_.output_.size()) {
    return;
  }
  compiler_.debugMap_.write<uint32_t>(moduleInfo_.bytecodePosOfLastParsedInstruction);
  compiler_.debugMap_.write<uint32_t>(moduleInfo_.outputSizeBeforeLastParsedInstruction);
}

void Frontend::patchDebugMapRef(uint32_t const debugMapRef) const VB_NOEXCEPT {
  uint8_t *const patchPtr{compiler_.debugMap_.posToPtr(debugMapRef)};
  uint32_t const fncDebugMachineCodeMapSize{compiler_.debugMap_.size() - (debugMapRef + 4U)};
  assert((fncDebugMachineCodeMapSize % 8U == 0) && "Machine code map not aligned to 8B");
  writeToPtr<uint32_t>(patchPtr, fncDebugMachineCodeMapSize / 8U);
}

void Frontend::writePaddedBinaryBlob(FunctionRef<void()> const &lambda) {
  uint32_t const wrapperStart{compiler_.output_.size()};
  lambda();                                                            // OPBVEF0
  uint32_t const wrapperSize{compiler_.output_.size() - wrapperStart}; //
  compiler_.backend_.execPadding(deltaToNextPow2(wrapperSize, 2U));    // OPBVEF1
  compiler_.output_.write<uint32_t>(wrapperSize);                      // OPBVEF2
}
uint32_t Frontend::getSigIndexForBlock(WasmType const wasmType) const VB_NOEXCEPT {
  switch (wasmType) {
  case WasmType::F64:
    return moduleInfo_.numTypes + 4U;
  case WasmType::F32:
    return moduleInfo_.numTypes + 3U;
  case WasmType::I64:
    return moduleInfo_.numTypes + 2U;
  case WasmType::I32:
    return moduleInfo_.numTypes + 1U;
  case WasmType::TVOID:
    return moduleInfo_.numTypes;
  default:
    // GCOVR_EXCL_START
    UNREACHABLE(return 0U, "Can not get sigIndex for WasmType");
    // GCOVR_EXCL_STOP
  }
}
uint32_t Frontend::reduceTypeIndex(uint32_t const typeIndex) const {
  if (typeIndex >= moduleInfo_.numTypes) {
    throw ValidationException(ErrorCode::Function_type_out_of_bounds);
  }
  uint8_t const *const signatureStart{pAddI(moduleInfo_.types(), moduleInfo_.typeOffsets[typeIndex])};
  if (readFromPtr<SignatureType>(signatureStart) == SignatureType::FORWARD) {
    return readFromPtr<uint32_t>(pAddI(signatureStart, 1));
  }
  return typeIndex;
}

// Check if the current frame is unreachable
// Current frame is either the current block/loop/ifblock frame or the function frame if fno block/loop/ifblock
// structure is currently active
bool Frontend::currentFrameIsUnreachable() const VB_NOEXCEPT {
  Stack::iterator const lastBlock{moduleInfo_.fnc.lastBlockReference};
  return (lastBlock.isEmpty()) ? moduleInfo_.fnc.unreachable : lastBlock->data.blockInfo.blockUnreachable;
}

// Sets the current function or block unreachable, e.g. after an unconditional branch or return
// Setting a frame to unreachable evaluates all instructions (for example a division instruction can trap when dividing
// by zero) and then drops all variables
void Frontend::setCurrentFrameFormallyUnreachable() VB_NOEXCEPT {
  // Retrieve the last block
  Stack::iterator const lastBlock{moduleInfo_.fnc.lastBlockReference};

  // Check if we have an open block or not
  if (!lastBlock.isEmpty()) {
    // Drop all elements in current frame incl. validation
    while (stack_.last() != lastBlock) {
      common_.dropValentBlock();
    }
    lastBlock->data.blockInfo.blockUnreachable = true;
  } else {
    while (!stack_.empty()) {
      common_.dropValentBlock();
    }
    moduleInfo_.fnc.unreachable = true;
  }
}
void Frontend::cleanCurrentBlockOnUnreachable() VB_NOEXCEPT {
  Stack::iterator const lastBlock{moduleInfo_.fnc.lastBlockReference};
  if (!lastBlock.isEmpty()) {
    // Drop all elements in current block
    while (stack_.last() != lastBlock) {
      common_.dropValentBlock();
    }
  } else {
    while (!stack_.empty()) {
      common_.dropValentBlock();
    }
  }
}

// Retrieves the targeted StackElement (representing a block, e.g. loop or block) for a given branchDepth
Stack::iterator Frontend::findTargetBlock(uint32_t const branchDepth) const {
  Stack::iterator targetBlockElem{moduleInfo_.fnc.lastBlockReference};
  if ((!targetBlockElem.isEmpty()) && (targetBlockElem->type == StackType::IFBLOCK)) {
    targetBlockElem = targetBlockElem->data.blockInfo.prevBlockReference; // skip IF-block
  }
  for (uint32_t i{0U}; i < branchDepth; i++) {
    if (targetBlockElem.isEmpty()) {
      throw ValidationException(ErrorCode::Invalid_branch_depth);
    }
    targetBlockElem = targetBlockElem->data.blockInfo.prevBlockReference;
    if ((!targetBlockElem.isEmpty()) && (targetBlockElem->type == StackType::IFBLOCK)) {
      targetBlockElem = targetBlockElem->data.blockInfo.prevBlockReference;
    }
  }
  return targetBlockElem;
}

Frontend::Frontend(Span<uint8_t const> const &bytecode, Span<NativeSymbol const> const &symbolList, ModuleInfo &moduleInfo, Stack &stack,
                   MemWriter &memory, Common &common, Compiler &compiler, ValidationStack &validationStack) VB_NOEXCEPT
    : br_{BytecodeReader(bytecode)},
      symbolList_{symbolList},
      moduleInfo_{moduleInfo},
      stack_{stack},
      memory_{memory},
      common_{common},
      compiler_{compiler},
      validationStack_{validationStack} {
}

// Wasm modules have to start with the wasm binary magic (number), make sure it does
void Frontend::validateMagicNumber() {
  constexpr std::array<uint8_t, 5U> wasmBinaryMagic{{0U, 0x61U, 0x73U, 0x6DU, 0U}};
  static_assert((wasmBinaryMagic.size() - 1U) == 4U, "Binary magic length needs to be four");
  while ((br_.getOffset()) < (wasmBinaryMagic.size() - 1U)) {
    size_t const offset{br_.getOffset()};
    if (wasmBinaryMagic[offset] != br_.readByte<uint8_t>()) {
      throw ValidationException(ErrorCode::Wrong_Wasm_magic_number);
    }
  }
}

// Compare the wasm module version to the supported version
void Frontend::validateVersion() {
  uint32_t const moduleWasmVersion{br_.readLEU32()};
  constexpr uint32_t supportedWasmVersion{1U};

  if (moduleWasmVersion != supportedWasmVersion) {
    if (compiler_.logging() != nullptr) {
      *compiler_.logging() << "Wasm Version" << moduleWasmVersion << "not supported" << &vb::endStatement<vb::LogLevel::LOGERROR>;
    }
    throw ImplementationLimitationException(ErrorCode::Wasm_Version_not_supported);
  }
}

// Parse "Type" section (Section ID = 1)
void Frontend::parseTypeSection() {
  moduleInfo_.numTypes = br_.readLEB128<uint32_t>();
  if (moduleInfo_.numTypes > ImplementationLimits::numTypes) {
    throw ImplementationLimitationException(ErrorCode::Too_many_types);
  }

  moduleInfo_.typeOffsets.setOffset(memory_.alignForType<uint32_t>(), memory_);

  // Skip and reserve space for offsets (+1 to have end index of last type)
  // (+5 to store types: ()=>(), ()=>I32, ()=>I64, ()=>F32, ()=>F64, used for `blockType ::= valtype?`
  memory_.step(((moduleInfo_.numTypes + 5U) + 1U) * static_cast<uint32_t>(sizeof(uint32_t)));

  moduleInfo_.types.setOffset(memory_.size(), memory_);
  for (uint32_t i{0U}; i < moduleInfo_.numTypes; i++) {
    uint8_t const typeType{br_.readByte<uint8_t>()};

    // Only function types supported
    if (typeType != 0x60U) {
      throw ValidationException(ErrorCode::Malformed_section_1__wrong_type);
    }

    // Write the signature of the function type to metadata memory
    moduleInfo_.typeOffsets[i] = memory_.size() - moduleInfo_.types.getOffset();

    memory_.write<SignatureType>(SignatureType::PARAMSTART);
    uint32_t const numParams{br_.readLEB128<uint32_t>()};
    if (numParams > ImplementationLimits::numParams) {
      throw ImplementationLimitationException(ErrorCode::Too_many_params);
    }
    for (uint32_t j{0U}; j < numParams; j++) {
      WasmType const type{br_.readByte<WasmType>()};
      if (!WasmTypeUtil::validateWasmType(type)) {
        throw ValidationException(ErrorCode::Invalid_function_parameter_type);
      }
      memory_.write<SignatureType>(WasmTypeUtil::toSignatureType(type));
    }
    memory_.write<SignatureType>(SignatureType::PARAMEND);

    // And the result types as part of the signature
    uint32_t const numResults{br_.readLEB128<uint32_t>()};
    if (numResults > ImplementationLimits::numResults) {
      throw ImplementationLimitationException(ErrorCode::Too_many_results);
    }
    for (uint32_t j{0U}; j < numResults; j++) {
      WasmType const type{br_.readByte<WasmType>()};
      if (!WasmTypeUtil::validateWasmType(type)) {
        throw ValidationException(ErrorCode::Invalid_function_return_type);
      }
      memory_.write<SignatureType>(WasmTypeUtil::toSignatureType(type));
    }

    // Check whether a previous signature matches and forward the type to that,
    // needed for indirect calls to matching signatures with different indices
    uint32_t const endOffset{memory_.size() - moduleInfo_.types.getOffset()};
    uint32_t const currentSignatureLength{endOffset - moduleInfo_.typeOffsets[i]};
    char const *const currentSignature{pCast<char const *>(pAddI(moduleInfo_.types(), moduleInfo_.typeOffsets[i]))};
    for (uint32_t j{0U}; j < i; j++) {
      uint32_t const iteratedSignatureLength{moduleInfo_.typeOffsets[j + 1U] - moduleInfo_.typeOffsets[j]};
      char const *const iteratedSignature{pCast<char const *>(pAddI(moduleInfo_.types(), moduleInfo_.typeOffsets[j]))};
      if ((iteratedSignatureLength == currentSignatureLength) &&
          // NOLINTNEXTLINE(clang-analyzer-core.NonNullParamChecker, clang-analyzer-unix.cstring.NullArg,-warnings-as-errors)
          (std::strncmp(currentSignature, iteratedSignature, static_cast<size_t>(currentSignatureLength)) == 0)) {
        memory_.resize(moduleInfo_.types.getOffset() + moduleInfo_.typeOffsets[i]);
        memory_.write<SignatureType>(SignatureType::FORWARD);
        memory_.write<uint32_t>(j);
        break;
      }
    }
  }
  // Write custom signature ()=>(), ()=>I32, ()=>I64, ()=>F32, ()=>F64
  for (uint32_t index{0U}; index < 5U; index++) {
    moduleInfo_.typeOffsets[moduleInfo_.numTypes + index] = memory_.size() - moduleInfo_.types.getOffset();
    memory_.write<SignatureType>(SignatureType::PARAMSTART);
    memory_.write<SignatureType>(SignatureType::PARAMEND);
    switch (index) {
    case 0: {
      break;
    }
    case 1: {
      memory_.write<SignatureType>(SignatureType::I32);
      break;
    }
    case 2: {
      memory_.write<SignatureType>(SignatureType::I64);
      break;
    }
    case 3: {
      memory_.write<SignatureType>(SignatureType::F32);
      break;
    }
    case 4: {
      memory_.write<SignatureType>(SignatureType::F64);
      break;
    }
    // GCOVR_EXCL_START
    default: {
      UNREACHABLE(return, "Unknown Custom Signature Index");
    }
      // GCOVR_EXCL_STOP
    }
  }

  // Write the offset to the types array
  moduleInfo_.typeOffsets[moduleInfo_.numTypes + 5U] = memory_.size() - moduleInfo_.types.getOffset();
}

// Parse "Import" section (Section ID = 2)
void Frontend::parseImportSection() {
  moduleInfo_.numImportedFunctions = 0U;
  uint32_t const numImports{br_.readLEB128<uint32_t>()};
  for (uint32_t i{0U}; i < numImports; i++) {
    uint32_t const moduleNameLength{br_.readLEB128<uint32_t>()}; // e.g. "env"
    if (moduleNameLength > ImplementationLimits::maxStringLength) {
      throw ImplementationLimitationException(ErrorCode::Module_name_too_long);
    }
    char const *const moduleName{pCast<char const *>(br_.getPtr())};
    br_.step(moduleNameLength);
    validateUTF8(moduleName, static_cast<size_t>(moduleNameLength));

    uint32_t const fieldNameLength{br_.readLEB128<uint32_t>()}; // e.g. "evaluate"
    if (fieldNameLength > ImplementationLimits::maxStringLength) {
      throw ImplementationLimitationException(ErrorCode::Import_name_too_long);
    }
    char const *const fieldName{pCast<char const *>(br_.getPtr())};
    br_.step(fieldNameLength);
    validateUTF8(fieldName, static_cast<size_t>(fieldNameLength));

    WasmImportExportType const importType{br_.readByte<WasmImportExportType>()};
    switch (importType) {
    case WasmImportExportType::FUNC: {
      // Importing a function
      // The function's signature index referencing the type index
      uint32_t const importSignatureIndex{reduceTypeIndex(br_.readLEB128<uint32_t>())};
      if (importSignatureIndex >= moduleInfo_.numTypes) {
        throw ValidationException(ErrorCode::Function_type_index_out_of_bounds);
      }

      // Retrieve the function type signature string
      uint32_t const typeOffset{moduleInfo_.typeOffsets[importSignatureIndex]};
      uint32_t const nextTypeOffset{moduleInfo_.typeOffsets[importSignatureIndex + 1U]};
      uint32_t const signatureLength{nextTypeOffset - typeOffset};
      char const *const signature{pAddI(pCast<char const *>(moduleInfo_.types()), typeOffset)};

#if BUILTIN_FUNCTIONS
      // Environment and function names for builtin functions
      constexpr std::array<char, 8> builtinModuleName{"builtin"};
      constexpr size_t builtinModuleNameLength{sizeof(builtinModuleName) - 1U};
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto builtinFunctionNames =
          make_array("trap", "getLengthOfLinkedMemory", "getU8FromLinkedMemory", "getI8FromLinkedMemory", "getU16FromLinkedMemory",
                     "getI16FromLinkedMemory", "getU32FromLinkedMemory", "getI32FromLinkedMemory", "getU64FromLinkedMemory", "getI64FromLinkedMemory",
                     "getF32FromLinkedMemory", "getF64FromLinkedMemory", "copyFromLinkedMemory", "isFunctionLinked", "tracePoint");
      // coverity[autosar_cpp14_a8_5_2_violation]
      constexpr auto builtinFunctionSignatures =
          make_array("()", "()i", "(i)i", "(i)i", "(i)i", "(i)i", "(i)i", "(i)i", "(i)I", "(i)I", "(i)f", "(i)F", "(iii)", "(i)i", "(i)");
      static_assert(builtinFunctionNames.size() == builtinFunctionSignatures.size(), "Builtin function names and signatures must match in size");
      // If module (environment) matches (compare length at first for
      // performance improvement)
      if ((builtinModuleNameLength == moduleNameLength) &&
          (std::strncmp(moduleName, builtinModuleName.data(), static_cast<size_t>(moduleNameLength)) == 0)) {
        BuiltinFunction builtinFunction{BuiltinFunction::UNDEFINED};

        constexpr uint32_t numBuiltinFunctionNames{sizeof(builtinFunctionNames) / sizeof(char *)};
        static_assert(numBuiltinFunctionNames <= static_cast<uint8_t>(BuiltinFunction::UNDEFINED), "Too many builtin functions");

        // Iterate through the list of builtin function names
        for (uint8_t j{0U}; j < numBuiltinFunctionNames; j++) {
          // Compare function name and function signature
          if ((fieldNameLength == strlen_s(builtinFunctionNames[j], static_cast<size_t>(ImplementationLimits::maxStringLength))) &&
              (std::strncmp(builtinFunctionNames[j], fieldName, static_cast<size_t>(fieldNameLength)) == 0)) {
            if ((signatureLength == strlen_s(builtinFunctionSignatures[j], static_cast<size_t>(ImplementationLimits::maxStringLength))) &&
                (std::strncmp(builtinFunctionSignatures[j], signature, static_cast<size_t>(signatureLength)) == 0)) {
              // If it matches, we save the builtin function index
              builtinFunction = static_cast<BuiltinFunction>(j);
              break;
            }
          }
        }

        // If a function has matched
        // We set the import function definition to reflect that, write it to
        // memory (we found an imported function since all builtins are)
        if (builtinFunction != BuiltinFunction::UNDEFINED) {
          ModuleInfo::ImpFuncDef impFuncDef{};
          impFuncDef.sigIndex = importSignatureIndex;
          impFuncDef.builtinFunction = builtinFunction;
          impFuncDef.linked = true;
          memory_.write<ModuleInfo::ImpFuncDef>(impFuncDef);
          moduleInfo_.numImportedFunctions++;
          // handle next import
          continue;
        }
      }
#endif

      bool foundImport{false};
      // No builtin function has matched yet, so iterate through the provided
      // list of importable functions by the embedder
      for (uint32_t symbolIndex{0U}; symbolIndex < symbolList_.size(); symbolIndex++) {
        // Retrieve the symbol and length
        NativeSymbol const symbol{symbolList_[symbolIndex]};
        size_t const symbolNameLength{strlen_s(symbol.symbol, static_cast<size_t>(ImplementationLimits::maxStringLength))};
        size_t const symbolModuleNameLength{strlen_s(symbol.moduleName, static_cast<size_t>(ImplementationLimits::maxStringLength))};

        // ... must match
        if ((symbolModuleNameLength != moduleNameLength) || (symbolNameLength != fieldNameLength)) {
          continue;
        }

        // If the module name and symbol name match
        if ((std::strncmp(moduleName, symbol.moduleName, static_cast<size_t>(moduleNameLength)) == 0) &&
            (std::strncmp(fieldName, symbol.symbol, static_cast<size_t>(fieldNameLength)) == 0)) {
          // And also if the signature length matches and the signature string matches, we have found a match
          if ((signatureLength == strlen_s(symbol.signature, static_cast<size_t>(ImplementationLimits::maxStringLength))) &&
              (std::strncmp(symbol.signature, signature, static_cast<size_t>(signatureLength)) == 0)) {
            ModuleInfo::ImpFuncDef impFuncDef{};
            impFuncDef.symbolIndex = symbolIndex;
            impFuncDef.linkDataOffset = moduleInfo_.linkDataLength;
            impFuncDef.sigIndex = importSignatureIndex;
            impFuncDef.builtinFunction = BuiltinFunction::UNDEFINED;
            impFuncDef.linked = true;
            impFuncDef.importFnVersion = symbol.importVersion;
            memory_.write<ModuleInfo::ImpFuncDef>(impFuncDef);
            moduleInfo_.numImportedFunctions++;

            // If it's dynamically imported, we increment by funcPtrSize after
            // allocating since the pointer needs to be stored in the
            // linkData
            if (symbol.linkage == NativeSymbol::Linkage::DYNAMIC) {
              moduleInfo_.linkDataLength += static_cast<uint32_t>(sizeof(void (*)(void)));
            }

            foundImport = true;
            break;
          }
        }
      }

      if (!foundImport) {
        if (compiler_.allowUnknownImports_) {
          if (compiler_.logging() != nullptr) {
            *compiler_.logging() << "Linking failed for: " << Span<char const>(moduleName, moduleNameLength) << " "
                                 << Span<char const>(fieldName, fieldNameLength) << ". Calling this function will lead to a trap."
                                 << Span<char const>(signature, signatureLength) << &vb::endStatement<vb::LogLevel::LOGWARNING>;
          }

          ModuleInfo::ImpFuncDef impFuncDef{};
          impFuncDef.symbolIndex = 0U;
          impFuncDef.linkDataOffset = 0U;
          impFuncDef.sigIndex = importSignatureIndex;
          impFuncDef.builtinFunction = BuiltinFunction::UNDEFINED;
          impFuncDef.linked = false;
          memory_.write<ModuleInfo::ImpFuncDef>(impFuncDef);
          moduleInfo_.numImportedFunctions++;
        } else {
          static_assert(sizeof(int32_t) <= sizeof(int), "Int size not suited");
          if (compiler_.logging() != nullptr) {
            *compiler_.logging() << "Linking failed for: " << Span<char const>(moduleName, moduleNameLength) << " "
                                 << Span<char const>(fieldName, fieldNameLength) << " " << Span<char const>(signature, signatureLength)
                                 << &vb::endStatement<vb::LogLevel::LOGERROR>;
          }
          throw LinkingException(ErrorCode::Imported_symbol_could_not_be_found);
        }
      }

      break;
    }
    case WasmImportExportType::TABLE:
      throw FeatureNotSupportedException(ErrorCode::Imported_table_not_supported);
    case WasmImportExportType::MEM:
      throw FeatureNotSupportedException(ErrorCode::Imported_memory_not_supported);
    case WasmImportExportType::GLOBAL:
      throw FeatureNotSupportedException(ErrorCode::Imported_global_not_supported);
    default:
      throw ValidationException(ErrorCode::Unknown_import_type);
    }
  }
  moduleInfo_.numTotalFunctions = moduleInfo_.numImportedFunctions;
  if (moduleInfo_.numImportedFunctions > ImplementationLimits::numImportedFunctions) {
    throw ImplementationLimitationException(ErrorCode::Too_many_imported_functions);
  }
}

// Parse "Function" section (Section ID = 3)

// Parse the function section if it's there, simply contains a list of
// functions that are defined in the Wasm module (non-imported) and their
// signature/type index
void Frontend::parseFunctionSection() {
  uint32_t const numNonImportedFunctions{br_.readLEB128<uint32_t>()};
  if (numNonImportedFunctions > ImplementationLimits::numNonImportedFunctions) {
    throw ImplementationLimitationException(ErrorCode::Maximum_number_of_functions_exceeded);
  }

  // Total number of functions is sum of direct (in-module-defined) functions and the number of imported functions
  moduleInfo_.numTotalFunctions = moduleInfo_.numImportedFunctions + numNonImportedFunctions;

  for (uint32_t i{0U}; i < numNonImportedFunctions; i++) {
    // Signature/type index for this function
    uint32_t const functionTypeIndex{reduceTypeIndex(br_.readLEB128<uint32_t>())};
    ModuleInfo::FuncDef funcDef{};
    funcDef.sigIndex = functionTypeIndex;
    memory_.write<ModuleInfo::FuncDef>(funcDef);
  }
}

// Parse table section if it's there
void Frontend::parseTableSection() {
  uint32_t const numTables{br_.readLEB128<uint32_t>()};

  // So far, even the spec only allows a single table section
  if (numTables > 1U) {
    throw FeatureNotSupportedException(ErrorCode::Reference_type_feature_not_implemented);
  }
  moduleInfo_.hasTable = true;
  for (uint32_t i{0U}; i < numTables; i++) {
    WasmType const elementType{br_.readByte<WasmType>()};
    if (!WasmTypeUtil::isRefType(elementType)) {
      throw ValidationException(ErrorCode::Only_table_type__funcref__allowed);
    }
    if (elementType != WasmType::FUNC_REF) {
      throw FeatureNotSupportedException(ErrorCode::Reference_type_feature_not_implemented);
    }
    uint8_t const hasSizeLimit{br_.readByte<uint8_t>()};

    // hasSizeLimit flag is only allowed to be 0 = false or 1 = true
    if ((hasSizeLimit != 0U) && (hasSizeLimit != 1U)) {
      throw ValidationException(ErrorCode::Unknown_size_limit_flag);
    }

    // Convert to bool
    moduleInfo_.tableHasSizeLimit = hasSizeLimit != 0U;
    moduleInfo_.tableInitialSize = br_.readLEB128<uint32_t>();
    if (moduleInfo_.tableInitialSize > ImplementationLimits::numTableEntries) {
      throw ImplementationLimitationException(ErrorCode::Table_initial_size_too_long);
    }

    if (moduleInfo_.tableHasSizeLimit) {
      moduleInfo_.tableMaximumSize = br_.readLEB128<uint32_t>();
      if (moduleInfo_.tableMaximumSize < moduleInfo_.tableInitialSize) {
        throw ValidationException(ErrorCode::Maximum_table_size_smaller_than_initial_table_size);
      }
      if (moduleInfo_.tableMaximumSize > ImplementationLimits::numTableEntries) {
        throw ImplementationLimitationException(ErrorCode::Table_Maximum_Size_too_long);
      }
    }

    // Initialize space in memory for storing all table elements, even if not
    // all table elements are defined
    moduleInfo_.tableElements.setOffset(memory_.alignForType<ModuleInfo::TableElement>(), memory_);
    memory_.step(moduleInfo_.tableInitialSize * static_cast<uint32_t>(sizeof(ModuleInfo::TableElement)));
    for (uint32_t j{0U}; j < moduleInfo_.tableInitialSize; j++) {
      // Dummy (unknown) function index, initialize all elements with
      // unknownValues so they can later be differentiated from an actual
      // function index without needing another bool flag determining whether
      // the value has been set
      constexpr uint32_t unknownValue{0xFFFFFFFFU};
      moduleInfo_.tableElements[j].fncIndex = unknownValue;
      moduleInfo_.tableElements[j].exportWrapperOffset = unknownValue;
    }
  }
}

// Parse memory section, defining a memory (if it's there)
void Frontend::parseMemorySection() {
  uint32_t const numMemories{br_.readLEB128<uint32_t>()};
  // Only one memory currently allowed, even according to the spec
  if (numMemories > 1U) {
    throw ValidationException(ErrorCode::Only_one_memory_instance_allowed);
  }
  for (uint32_t i{0U}; i < numMemories; i++) {
    moduleInfo_.hasMemory = true;
    uint8_t const hasSizeLimit{br_.readByte<uint8_t>()};

    // Only 0 and 1 allowed
    if ((hasSizeLimit != 0U) && (hasSizeLimit != 1U)) {
      throw ValidationException(ErrorCode::Unknown_size_limit_flag);
    }

    // Convert to bool
    moduleInfo_.memoryHasSizeLimit = hasSizeLimit != 0U;
    moduleInfo_.memoryInitialSize = br_.readLEB128<uint32_t>();

    if (moduleInfo_.memoryHasSizeLimit) {
      moduleInfo_.memoryMaximumSize = br_.readLEB128<uint32_t>();
      if (moduleInfo_.memoryMaximumSize < moduleInfo_.memoryInitialSize) {
        throw ValidationException(ErrorCode::Maximum_memory_size_smaller_than_initial_memory_size);
      }
    }

    if ((moduleInfo_.memoryInitialSize > 65536U) || (moduleInfo_.memoryHasSizeLimit && (moduleInfo_.memoryMaximumSize > 65536U))) {
      throw ValidationException(ErrorCode::Memory_size_must_be_at_most_65536_pages__4GiB_);
    }
  }

  uint32_t const bytesForAlignment{deltaToNextPow2(compiler_.output_.size(), 2U)};
  compiler_.output_.step(bytesForAlignment);
}

/// @brief parse op code
/// @param br bytecode reader in @b Frontend
static OPCode parseOpCode(BytecodeReader &br) {
  OPCode ret;
  uint8_t const firstByte{br.readByte<uint8_t>()};
  if ((firstByte == static_cast<uint8_t>(OPCode::SCALAR_EXTEND_OP_CODE)) || (firstByte == static_cast<uint8_t>(OPCode::VECTOR_EXTEND_OP_CODE))) {
    uint32_t const extendOpCode{br.readLEB128<uint32_t>()};
    if (extendOpCode > static_cast<uint32_t>(std::numeric_limits<uint8_t>::max())) {
      throw ValidationException(ErrorCode::Unknown_instruction);
    }
    // coverity[autosar_cpp14_a7_2_1_violation]
    ret = static_cast<OPCode>(static_cast<uint16_t>(OPCode::SCALAR_EXTEND_OP_CODE_PREFIX) | static_cast<uint16_t>(extendOpCode));
    if (firstByte == static_cast<uint8_t>(OPCode::VECTOR_EXTEND_OP_CODE)) {
      throw FeatureNotSupportedException(ErrorCode::Simd_feature_not_implemented);
    }
  } else {
    // coverity[autosar_cpp14_a7_2_1_violation]
    ret = static_cast<OPCode>(static_cast<uint16_t>(firstByte));
  }
  return ret;
}

// Parse global section (section defining global variables; if it's there)
void Frontend::parseGlobalSection() {
  moduleInfo_.globals.setOffset(memory_.alignForType<ModuleInfo::GlobalDef>(), memory_);
  moduleInfo_.numNonImportedGlobals = br_.readLEB128<uint32_t>();
  if (moduleInfo_.numNonImportedGlobals > ImplementationLimits::numNonImportedGlobals) {
    throw ImplementationLimitationException(ErrorCode::Too_many_globals);
  }

  // Reserve memory space for all global definitions
  memory_.step(moduleInfo_.numNonImportedGlobals * static_cast<uint32_t>(sizeof(ModuleInfo::GlobalDef)));

  for (uint32_t i{0U}; i < moduleInfo_.numNonImportedGlobals; i++) {
    // Write where variable would be stored, even if it's immutable. In that
    // case this is not relevant anyway
    ModuleInfo::GlobalDef &globalDef{moduleInfo_.globals[i]};
    globalDef.linkDataOffset = moduleInfo_.linkDataLength;
    WasmType const wasmType{br_.readByte<WasmType>()};
    if (!WasmTypeUtil::validateWasmType(wasmType)) {
      throw ValidationException(ErrorCode::Invalid_global_type);
    }
    globalDef.type = MachineTypeUtil::from(wasmType);
    uint8_t const isMutable{br_.readByte<uint8_t>()};
    if ((isMutable != 0U) && (isMutable != 1U)) {
      throw ValidationException(ErrorCode::Unknown_mutability_flag);
    }
    globalDef.isMutable = isMutable != 0U; // Convert to bool

    if (((!compiler_.getDebugMode()) && (i == 0U)) && (globalDef.isMutable && (globalDef.type == MachineType::I32))) {
      globalDef.reg = compiler_.backend_.allocateRegForGlobal(globalDef.type);
      assert((globalDef.reg != TReg::NONE) && "failed to allocate reg for global");
    } else {
      globalDef.reg = TReg::NONE;
    }

    // Look for the initialization instruction
    OPCode instruction{parseOpCode(br_)};
    if ((instruction >= OPCode::I32_CONST) && (instruction <= OPCode::F64_CONST)) {
      if ((globalDef.type == MachineType::I32) && (instruction == OPCode::I32_CONST)) {
        globalDef.initialValue.u32 = bit_cast<uint32_t>(br_.readLEB128<int32_t>());
      } else if ((globalDef.type == MachineType::I64) && (instruction == OPCode::I64_CONST)) {
        globalDef.initialValue.u64 = bit_cast<uint64_t>(br_.readLEB128<int64_t>());
      } else if ((globalDef.type == MachineType::F32) && (instruction == OPCode::F32_CONST)) {
        globalDef.initialValue.f32 = bit_cast<float>(br_.readLEU32());
      } else if ((globalDef.type == MachineType::F64) && (instruction == OPCode::F64_CONST)) {
        globalDef.initialValue.f64 = bit_cast<double>(br_.readLEU64());
      } else {
        throw ValidationException(ErrorCode::Malformed_global_initialization_expression);
      }
      instruction = parseOpCode(br_);
      if (instruction != OPCode::END) {
        throw ValidationException(ErrorCode::Malformed_global_initialization_expression);
      }
    } else if (instruction == OPCode::GLOBAL_GET) {
      throw FeatureNotSupportedException(ErrorCode::Imported_globals_not_supported);
    } else {
      throw ValidationException(ErrorCode::Malformed_global_initialization_expression);
    }

    // Allocate only 4-byte sized globals in first round, because they will be
    // definitely naturally-aligned. Before that, only pointers are saved to an
    // at least 8-byte aligned memory location. Pointer can be either 4 byte or
    // 8 byte of size, depending on the architecture. Then allocate only 8-byte
    // sized globals in second round
    uint32_t const size{MachineTypeUtil::getSize(globalDef.type)};
    // If mutable, there is actually some variable so we need to increment the
    // linkDataLength
    if ((isMutable != 0U) && (size == 4U)) {
      moduleInfo_.linkDataLength += 4U;
    }
    // Write global definition to pre-reserved memory
  }

  // Go through the globals again and update the link data offset for all 8
  // byte wide values Round up to 8 bytes, so the upcoming value will be aligned
  // again.
  moduleInfo_.linkDataLength = roundUpToPow2(moduleInfo_.linkDataLength, 3U);
  for (uint32_t i{0U}; i < moduleInfo_.numNonImportedGlobals; i++) {
    ModuleInfo::GlobalDef &globalDef{moduleInfo_.globals[i]};
    uint32_t const size{MachineTypeUtil::getSize(globalDef.type)};
    if (globalDef.isMutable && (size == 8U)) {
      globalDef.linkDataOffset = moduleInfo_.linkDataLength;
      moduleInfo_.linkDataLength += 8U;
    }
  }
  // Now the memory is again guaranteed to be 8-byte aligned
}

// Parse export section if it's present
void Frontend::parseExportSection() {
  moduleInfo_.exports.setOffset(memory_.size(), memory_);
  moduleInfo_.numExports = br_.readLEB128<uint32_t>();
  for (uint32_t i{0U}; i < moduleInfo_.numExports; i++) {
    uint32_t const exportNameLength{br_.readLEB128<uint32_t>()};
    if (exportNameLength > ImplementationLimits::maxStringLength) {
      throw ImplementationLimitationException(ErrorCode::Export_name_too_long);
    }

    char const *const exportName{pCast<char const *>(br_.getPtr())};

    // Reserve space for export name
    br_.step(exportNameLength);
    validateUTF8(exportName, static_cast<size_t>(exportNameLength));
    // Write the name length as u32 to output binary
    memory_.write<uint32_t>(exportNameLength);

    // Reserve n bytes and write the export name there
    memory_.step(exportNameLength);
    static_cast<void>(std::memcpy(pSubI(memory_.ptr(), exportNameLength), exportName, static_cast<size_t>(exportNameLength)));

    WasmImportExportType const exportType{br_.readByte<WasmImportExportType>()};
    memory_.write<WasmImportExportType>(exportType);
    if (!(exportType <= WasmImportExportType::GLOBAL)) {
      throw ValidationException(ErrorCode::Unknown_export_type);
    }

    uint32_t const index{br_.readLEB128<uint32_t>()};
    memory_.write<uint32_t>(index);

    // wasmFncBodyBinaryPositions contains binary offsets ("compressed
    // pointers") to the start of all functions following the calling convention
    if (exportType == WasmImportExportType::FUNC) {
      if (index >= moduleInfo_.numTotalFunctions) {
        throw ValidationException(ErrorCode::Function_out_of_range);
      }
    } else if (exportType == WasmImportExportType::GLOBAL) {
      if (index >= moduleInfo_.numNonImportedGlobals) {
        throw ValidationException(ErrorCode::Global_out_of_range);
      }
    } else if (exportType == WasmImportExportType::MEM) {
      if ((!moduleInfo_.hasMemory) || (index > 0U)) {
        throw ValidationException(ErrorCode::Memory_out_of_range);
      }
      moduleInfo_.memoryIsExported = true;
    } else if (exportType == WasmImportExportType::TABLE) {
      if ((!moduleInfo_.hasTable) || (index > 0U)) {
        throw ValidationException(ErrorCode::Table_out_of_range);
      }
      moduleInfo_.tableIsExported = true;
    } else {
      // coverity[autosar_cpp14_m0_1_2_violation]
      // coverity[autosar_cpp14_m0_1_9_violation]
      static_cast<void>(0);
    }

    // Check for duplicate export names
    uint8_t const *stepPtr{moduleInfo_.exports()};
    for (uint32_t j{0U}; j < i; j++) {
      uint32_t const iteratedExportNameLength{readFromPtr<uint32_t>(stepPtr)};
      stepPtr = pAddI(stepPtr, 4U);

      char const *const iteratedExportName{pCast<char const *>(stepPtr)};
      stepPtr = pAddI(stepPtr, iteratedExportNameLength);

      if ((iteratedExportNameLength == exportNameLength) &&
          (std::strncmp(iteratedExportName, exportName, static_cast<size_t>(exportNameLength)) == 0)) {
        throw ValidationException(ErrorCode::Duplicate_export_symbol);
      }

      // Skip exportType and index
      stepPtr = pAddI(stepPtr, 1);
      stepPtr = pAddI(stepPtr, 4);
    }
  }
}

// Parse start section (if defined, the module has a start function which will
// be executed as an initialization step)
void Frontend::parseStartSection() {
  moduleInfo_.hasStartFunction = true;
  moduleInfo_.startFunctionIndex = br_.readLEB128<uint32_t>();
  if (moduleInfo_.startFunctionIndex >= moduleInfo_.numTotalFunctions) {
    throw ValidationException(ErrorCode::Start_function_index_out_of_range);
  }

  // Validate start function signature
  uint32_t const sigIndex{moduleInfo_.getFncSigIndex(moduleInfo_.startFunctionIndex)};
  uint32_t const typeOffset{moduleInfo_.typeOffsets[sigIndex]};
  uint32_t const nextTypeOffset{moduleInfo_.typeOffsets[sigIndex + 1U]};
  uint32_t const signatureLength{nextTypeOffset - typeOffset};

  // Signature for nullary function must be "()", i.e. no params and no return value
  if (signatureLength != 2U) {
    throw ValidationException(ErrorCode::Start_function_must_be_nullary);
  }

  // Start function is an imported function, Has not been generated yet (would
  // be the case if they are also exported or if one function is in the table
  // multiple times) Save current offset as wrapper start, produce the wrapper
  if (moduleInfo_.functionIsImported(moduleInfo_.startFunctionIndex)) {
    if (moduleInfo_.wasmFncBodyBinaryPositions[moduleInfo_.startFunctionIndex] == 0xFFFFFFFFU) {
      moduleInfo_.wasmFncBodyBinaryPositions[moduleInfo_.startFunctionIndex] = compiler_.output_.size();
      writePaddedBinaryBlob(FunctionRef<void()>([this]() {
        compiler_.backend_.emitWasmToNativeAdapter(moduleInfo_.startFunctionIndex);
      })); // OPBVF0
    }
  }
}

// Parse element section if present
void Frontend::parseElementSection() {
  uint32_t const numElementSegments{br_.readLEB128<uint32_t>()};
  for (uint32_t i{0U}; i < numElementSegments; i++) { // Number of table segments
    enum class ElementMode : uint8_t { LegacyIndex, PassiveIndex, ActiveIndex, DeclaredIndex, LegacyExpr, PassiveExpr, ActiveExpr, DeclaredExpr };
    // coverity[autosar_cpp14_a7_2_1_violation]
    ElementMode const mode{static_cast<ElementMode>(br_.readLEB128<uint32_t>())};
    if (mode != ElementMode::LegacyIndex) {
      throw FeatureNotSupportedException(ErrorCode::Bulk_memory_operations_feature_not_implemented);
    }
    if (!moduleInfo_.hasTable) {
      throw ValidationException(ErrorCode::Table_index_out_of_bounds);
    }

    OPCode instruction{parseOpCode(br_)};
    uint32_t offset;
    if ((instruction >= OPCode::I32_CONST) && (instruction <= OPCode::F64_CONST)) {
      if (instruction != OPCode::I32_CONST) {
        throw ValidationException(ErrorCode::Constant_expression_offset_has_to_be_of_type_i32);
      }
      offset = br_.readLEB128<uint32_t>();
      instruction = parseOpCode(br_);
      if (instruction != OPCode::END) {
        throw ValidationException(ErrorCode::Malformed_constant_expression_offset);
      }
    } else if (instruction == OPCode::GLOBAL_GET) {
      throw FeatureNotSupportedException(ErrorCode::Imported_globals_not_supported);
    } else {
      throw ValidationException(ErrorCode::Malformed_constant_expression_offset);
    }

    // Number of actual table elements (function pointers/references) within the
    // actual segment
    uint32_t const numElements{br_.readLEB128<uint32_t>()};
    if ((static_cast<uint64_t>(offset) + numElements) > moduleInfo_.tableInitialSize) {
      throw ValidationException(ErrorCode::Table_element_index_out_of_range__initial_table_size_);
    }

    for (uint32_t j{0U}; j < numElements; j++) {
      uint32_t const elementFunctionIndex{br_.readLEB128<uint32_t>()};
      if (elementFunctionIndex >= moduleInfo_.numTotalFunctions) {
        throw ValidationException(ErrorCode::Function_index_out_of_range);
      }

      moduleInfo_.tableElements[offset + j].fncIndex = elementFunctionIndex;

      // Produce wrapper for imported functions present in table so they can be
      // indirectly called via the Wasm calling convention, the native Wasm
      // functions will be generated later anyway All functions (be it imported
      // or in-Wasm-defined) must be callable with the same calling convention,
      // since during function setup/compile-time it is not known which function
      // will actually be called So we produce a wrapper translating the Wasm
      // calling convention to the native calling convention and calls the
      // native (imported) function This needs to be done for every imported
      // function that is present in the table wasmFncBodyBinaryPositions
      // contains binary offsets ("compressed pointers") to the start of all
      // functions following the calling convention. If it's 0xFFFFFFFF
      // (originally initialized as that, it has not been generated yet)

      // If it's an imported function and has not been generated yet
      // We save the current offset as wrapper start, produce the wrapper for
      // the function and align the next section to 4 bytes again
      if (moduleInfo_.functionIsImported(elementFunctionIndex)) {
        ModuleInfo::ImpFuncDef const impFuncDef{moduleInfo_.getImpFuncDef(elementFunctionIndex)};
        if (impFuncDef.importFnVersion == NativeSymbol::ImportFnVersion::V2) {
          // V2 import functions are not supported to indirect call yet
          throw FeatureNotSupportedException(ErrorCode::Not_implemented);
        }
        // Check if the imported function is linked. If not, we do not generate a wrapper for it.
        if (impFuncDef.linked && (moduleInfo_.wasmFncBodyBinaryPositions[elementFunctionIndex] == 0xFFFFFFFFU)) {
          moduleInfo_.wasmFncBodyBinaryPositions[elementFunctionIndex] = compiler_.output_.size();
          writePaddedBinaryBlob(FunctionRef<void()>([this, elementFunctionIndex]() {
            compiler_.backend_.emitWasmToNativeAdapter(elementFunctionIndex);
          })); // OPBVF0
        }
      }
    }
  }
}

// Now the code section (if present) defining the actual function bodies (i.e. logic)
void Frontend::parseCodeSection() {
#if LINEAR_MEMORY_BOUNDS_CHECKS
  // Needs to have linkDataLength already defined
  if (moduleInfo_.hasMemory) {
    writePaddedBinaryBlob(FunctionRef<void()>([this]() {
      compiler_.backend_.emitExtensionRequestFunction();
    })); // OPBVF0
  }
#else
  writePaddedBinaryBlob(FunctionRef<void()>([this]() {
    compiler_.backend_.emitLandingPad();
  })); // OPBVF0
       // work around for differences between clang format versions
#endif

  // Where metadata (and local definition) starts, for every function that follows
  moduleInfo_.localDefs.setOffset(memory_.alignForType<ModuleInfo::LocalDef>(), memory_);

  // The number of functions that will be defined here
  uint32_t const numNonImportedFunctions{br_.readLEB128<uint32_t>()};
  if (numNonImportedFunctions != (moduleInfo_.numTotalFunctions - moduleInfo_.numImportedFunctions)) {
    throw ValidationException(ErrorCode::Function_and_code_section__mismatch_of_number_of_definitions);
  }

  // Write debug information if needed
  if (compiler_.getDebugMode()) {
    writeDebugMapPreamble();
  }

  moduleInfo_.fnc.lastBlockReference = Stack::iterator();
  for (uint32_t fncIndex{moduleInfo_.numImportedFunctions}; fncIndex < moduleInfo_.numTotalFunctions; fncIndex++) {
    assert(moduleInfo_.fnc.lastBlockReference.isEmpty() && "Last block not NULL at function entry");

    // Reset memory where we write info about the function params and locals etc.
    memory_.resize(moduleInfo_.localDefs.getOffset());

    uint32_t const functionBodySize{br_.readLEB128<uint32_t>()};
    uint8_t const *const functionPosAfterSize{br_.getPtr()};

#if ENABLE_EXTENSIONS
    extension::IDwarf5Generator *const dwarfGenerator{compiler_.getDwarfGenerator()};
    if (dwarfGenerator != nullptr) {
      dwarfGenerator->startOp(static_cast<uint32_t>(br_.getOffset()));
      dwarfGenerator->startFunction(compiler_.output_.size());
    }
#endif

    // Reset/initialize function-specific properties of moduleInfo so we are
    // always working with a clean slate
    moduleInfo_.fnc = ModuleInfo::FunctionInfo{};
    moduleInfo_.fnc.index = fncIndex;

    // Params
    ModuleInfo::FuncDef const funcDef{moduleInfo_.getFuncDef(moduleInfo_.fnc.index)};
    uint32_t const numParams{moduleInfo_.getNumParamsForSignature(funcDef.sigIndex)};
    memory_.reserve(numParams * static_cast<uint32_t>(sizeof(ModuleInfo::LocalDef)));

    assert((moduleInfo_.fnc.stackFrameSize == 0U) && "stackFrameSize must be 0 here");

    // NOTE: Params limits already checked in Type section
    moduleInfo_.iterateParamsForSignature(funcDef.sigIndex, FunctionRef<void(MachineType)>([this](MachineType const machineType) {
                                            // Will write the LocalDef object to the end of the memory
                                            compiler_.backend_.allocateLocal(machineType, true);
                                          }));

    // Backend-specific return address width on stack
    moduleInfo_.fnc.stackFrameSize += roundUpToPow2(NBackend::returnAddrWidth, 3U);

    // Locals
    uint32_t const localDeclarationCount{br_.readLEB128<uint32_t>()};
    for (uint32_t i{0U}; i < localDeclarationCount; i++) {
      // Multiplicity of local (i.e. n times local with given WasmType)
      uint32_t const localTypeCount{br_.readLEB128<uint32_t>()};

      // Check whether number of direct locals will exceed the limit
      uint32_t const numDirectLocals{moduleInfo_.fnc.numLocals - moduleInfo_.fnc.numParams};
      // Must go through uint64_t because localTypeCount might already exceed ImplementationLimits::numDirectLocals
      if ((static_cast<uint64_t>(numDirectLocals) + localTypeCount) > ImplementationLimits::numDirectLocals) {
        throw ImplementationLimitationException(ErrorCode::Too_many_direct_locals);
      }

      WasmType const type{br_.readByte<WasmType>()};
      if (!WasmTypeUtil::validateWasmType(type)) {
        throw ValidationException(ErrorCode::Invalid_local_type_in_function);
      }

      // Will write the LocalDef object to the end of the memory
      compiler_.backend_.allocateLocal(MachineTypeUtil::from(type), false, localTypeCount);
    }

    // The compiler_.backend.allocateLocal() calls will have moved the end
    // pointer of the memory, check if the expected value matches the actual value
    assert(memory_.ptr() == pCast<uint8_t *>(moduleInfo_.localDefs()) + sizeof(ModuleInfo::LocalDef) * moduleInfo_.fnc.numLocals &&
           "Incorrect number of locals allocated");
    assert(moduleInfo_.fnc.stackFrameSize == moduleInfo_.getFixedStackFrameWidth() && "StackFrameSize unaligned");

    for (uint32_t i{0U}; i < moduleInfo_.fnc.numLocals; i++) {
      if (moduleInfo_.localDefs[i].reg != TReg::NONE) {
        moduleInfo_.fnc.directLocalsWidth += 8U;
        moduleInfo_.fnc.stackFrameSize += 8U;
        moduleInfo_.localDefs[i].stackFramePosition = moduleInfo_.fnc.stackFrameSize;
      }
    }

    // Reset the stackFrameSize so that the backend can actually allocate it
    moduleInfo_.fnc.stackFrameSize -= moduleInfo_.fnc.directLocalsWidth;

    // OPBVF0
    // Notify the backend that we are entering a new function body to allow it
    // to go back and patch forward branches to that function body
    compiler_.backend_.enteredFunction();
#if INTERRUPTION_REQUEST
    compiler_.backend_.checkForInterruptionRequest();
#endif
    assert(static_cast<uint64_t>(memory_.ptr() - pCast<uint8_t *>(moduleInfo_.referencesToLastOccurrenceOnStack())) % sizeof(Stack::iterator) == 0 &&
           "VariableIndex end unaligned");
    uint32_t const numVariableIndices{(memory_.size() - moduleInfo_.referencesToLastOccurrenceOnStack.getOffset()) /
                                      static_cast<uint32_t>(sizeof(Stack::node))};

    // Reset the stack memory
    stack_.reset();
    // Reset validation stack
    validationStack_.reset(funcDef.sigIndex);

    // Write debug information if needed
    uint32_t debugMapRef{0U};
    if (compiler_.getDebugMode()) {
      debugMapRef = writeDebugMapFunctionInfo(fncIndex);
    }

    // Iterate over the function body instruction by instruction
    bool breakOutOfLoop{false};
    while (br_.getPtr() < pAddI(functionPosAfterSize, functionBodySize)) {
      if (breakOutOfLoop) {
        break;
      }
      size_t const bytecodePosition{br_.getOffset()};
      if (bytecodePosition > UINT32_MAX) {
        throw RuntimeError(ErrorCode::Maximum_offset_reached);
      }
      moduleInfo_.bytecodePosOfLastParsedInstruction = static_cast<uint32_t>(bytecodePosition);
      moduleInfo_.outputSizeBeforeLastParsedInstruction = compiler_.output_.size();
      OPCode const instruction{parseOpCode(br_)};
#if ENABLE_EXTENSIONS
      if (compiler_.getDwarfGenerator() != nullptr) {
        compiler_.getDwarfGenerator()->startOp(static_cast<uint32_t>(bytecodePosition));
      }
#endif
      switch (instruction) {
      case OPCode::UNREACHABLE: {
        if (!(currentFrameIsUnreachable())) {
          common_.condenseSideEffectInstructionToFrameBase();
        }

        validationStack_.markCurrentBlockUnreachable();

        if (!currentFrameIsUnreachable()) {
          // Unreachable is equivalent to a trap
          compiler_.backend_.executeTrap(TrapCode::UNREACHABLE);
        }

        // ... and after that, the current frame (either a block/loop frame or
        // the function frame itself) is unreachable (see spec validation rules)
        setCurrentFrameFormallyUnreachable();
        break;
      }

      case OPCode::NOP: {
        break;
      }

      case OPCode::BLOCK: {
        uint8_t const *const startPos{br_.getPtr()};
        WasmType const returnType{br_.readByte<WasmType>()};

        bool const isBasicType{WasmTypeUtil::validateWasmType(returnType, true)};
        // prepare sigIndex
        uint32_t sigIndex{0U};
        if (isBasicType) {
          sigIndex = getSigIndexForBlock(returnType);
        } else {
          br_.jumpTo(startPos);
          sigIndex = reduceTypeIndex(br_.readLEB128<uint32_t>());
        }
        validationStack_.validateAndPrepareBlock(sigIndex);

        bool const originalFrameUnreachable{currentFrameIsUnreachable()};
        if (originalFrameUnreachable) {
          pushDummyParamsOnUnreachable(sigIndex);
        }
        if (isBasicType) {
          // Notify the backend that we're entering a block so it can do stuff,
          // e.g. spill all scratch registers, NOTE: Not unreachable anymore
          // because we just entered a new frame
          common_.condenseSideEffectInstructionToFrameBase();
          compiler_.backend_.spillAllVariables();
          uint32_t const blockResultsStackOffset{common_.getCurrentMaximumUsedStackFramePosition()};
          Stack::iterator const prevBlockReference{moduleInfo_.fnc.lastBlockReference};
          moduleInfo_.fnc.lastBlockReference = stack_.push(StackElement::block(UnknownIndex, blockResultsStackOffset, prevBlockReference, sigIndex,
                                                                               moduleInfo_.fnc.stackFrameSize, originalFrameUnreachable));
        } else {
          uint32_t const numBlockParams{moduleInfo_.getNumParamsForSignature(sigIndex)};
          Stack::iterator paramsBase{stack_.end()};
          common_.condenseSideEffectInstructionBlewValentBlock(numBlockParams);
          if (numBlockParams > 0U) {
            paramsBase = common_.condenseMultipleValentBlocksBelow(stack_.end(), numBlockParams);
          }
          compiler_.backend_.spillAllVariables(paramsBase);

          uint32_t const returnValueStackWidth{common_.getStackReturnValueWidth(sigIndex)};
          uint32_t const blockResultsStackOffset{(returnValueStackWidth == 0U) ? common_.getCurrentMaximumUsedStackFramePosition()
                                                                               : compiler_.backend_.reserveStackFrame(returnValueStackWidth)};
          Stack::iterator const prevBlockReference{moduleInfo_.fnc.lastBlockReference};
          moduleInfo_.fnc.lastBlockReference =
              stack_.insert(paramsBase, StackElement::block(UnknownIndex, blockResultsStackOffset, prevBlockReference, sigIndex,
                                                            moduleInfo_.fnc.stackFrameSize, originalFrameUnreachable));
        }
        break;
      }

      case OPCode::LOOP: {
        uint8_t const *const startPos{br_.getPtr()};
        WasmType const returnType{br_.readByte<WasmType>()};

        bool const isBasicType{WasmTypeUtil::validateWasmType(returnType, true)};
        // prepare sigIndex
        uint32_t sigIndex{0U};
        if (isBasicType) {
          sigIndex = getSigIndexForBlock(returnType);
        } else {
          br_.jumpTo(startPos);
          sigIndex = reduceTypeIndex(br_.readLEB128<uint32_t>());
        }
        validationStack_.validateAndPrepareLoop(sigIndex);

        bool const originalFrameUnreachable{currentFrameIsUnreachable()};
        if (originalFrameUnreachable) {
          pushDummyParamsOnUnreachable(sigIndex);
        }
        if (isBasicType) {
          // Notify the backend that we're entering a block so it can do stuff,
          // e.g. spill all scratch registers, NOTE: Not unreachable anymore
          // because we just entered a new frame
          common_.condenseSideEffectInstructionToFrameBase();
          compiler_.backend_.spillAllVariables();
          common_.emitBranchMergePoint(!originalFrameUnreachable, nullptr);
          Stack::iterator const prevBlockReference{moduleInfo_.fnc.lastBlockReference};

          moduleInfo_.fnc.lastBlockReference =
              stack_.push(StackElement::loop(compiler_.output_.size(), common_.getCurrentMaximumUsedStackFramePosition(), prevBlockReference,
                                             sigIndex, moduleInfo_.fnc.stackFrameSize, originalFrameUnreachable));
        } else {
          uint32_t const numLoopParams{moduleInfo_.getNumParamsForSignature(sigIndex)};
          Stack::iterator paramsBase{};
          common_.condenseSideEffectInstructionBlewValentBlock(numLoopParams);
          if (numLoopParams > 0U) {
            paramsBase = common_.condenseMultipleValentBlocksWithTargetHintBelow(stack_.end(), sigIndex, true);
          }
          compiler_.backend_.spillAllVariables(paramsBase);
          common_.emitBranchMergePoint(!originalFrameUnreachable, nullptr);

          uint32_t const returnValueStackWidth{common_.getStackReturnValueWidth(sigIndex, true)};
          uint32_t const blockResultsStackOffset{(returnValueStackWidth == 0U) ? common_.getCurrentMaximumUsedStackFramePosition()
                                                                               : compiler_.backend_.reserveStackFrame(returnValueStackWidth)};
          StackElement loopElem{
              StackElement::loop(0U, blockResultsStackOffset, Stack::iterator(), sigIndex, moduleInfo_.fnc.stackFrameSize, originalFrameUnreachable)};

          // Physical data transfer
          if (numLoopParams > 0U) {
            common_.loadReturnValues(paramsBase, numLoopParams, &loopElem);
            common_.popReturnValueElems(paramsBase, numLoopParams);
          }

          loopElem.data.blockInfo.binaryPosition.loopStartOffset = compiler_.output_.size();
          loopElem.data.blockInfo.prevBlockReference = moduleInfo_.fnc.lastBlockReference;
          moduleInfo_.fnc.lastBlockReference = stack_.push(loopElem);

          // Logical stack representation
          if (numLoopParams > 0U) {
            NBackend::RegStackTracker tracker{};
            uint32_t const paramStackStartPosition{blockResultsStackOffset};
            moduleInfo_.iterateParamsForSignature(
                sigIndex, FunctionRef<void(MachineType)>([this, paramStackStartPosition, &tracker](MachineType const machineType) {
                  StackElement paramElem{};
                  TReg const targetReg{compiler_.backend_.getREGForReturnValue(machineType, tracker)};
                  if (targetReg != TReg::NONE) {
                    paramElem = StackElement::scratchReg(targetReg, MachineTypeUtil::toStackTypeFlag(machineType));
                  } else {
                    uint32_t const offsetFromSP{paramStackStartPosition - TBackend::offsetInStackReturnValues(tracker, machineType)};
                    paramElem = StackElement::tempResult(machineType, VariableStorage::stackMemory(machineType, offsetFromSP),
                                                         moduleInfo_.getStackMemoryReferencePosition());
                  }
                  common_.pushAndUpdateReference(paramElem);
                }));
          }
        }
#if INTERRUPTION_REQUEST
        compiler_.backend_.checkForInterruptionRequest();
#endif
        break;
      }

      case OPCode::IF: {
        uint8_t const *const startPos{br_.getPtr()};
        WasmType const returnType{br_.readByte<WasmType>()};

        bool const isBasicType{WasmTypeUtil::validateWasmType(returnType, true)};
        // prepare sigIndex
        uint32_t sigIndex{0U};
        if (isBasicType) {
          sigIndex = getSigIndexForBlock(returnType);
        } else {
          br_.jumpTo(startPos);
          sigIndex = reduceTypeIndex(br_.readLEB128<uint32_t>());
        }
        validationStack_.validateAndPrepareIfBlock(sigIndex);

        bool conditionCanBeEvaluatedAtCompileTime{false};
        bool conditionIsAlwaysTrue{false};

        BC branchCond{BC::UNCONDITIONAL};
        bool const originalFrameUnreachable{currentFrameIsUnreachable()};
        if (originalFrameUnreachable) {
          pushDummyParamsOnUnreachable(sigIndex);
          static_cast<void>(stack_.push(StackElement::dummyConst(MachineType::I32)));
        }

        // Push a (synthetic) block from the start of the if instruction to the
        // end of the else block, or if no else is present, to the end of the if
        // block body And push an inner if-block from the start of the if
        // instruction (right inside the synthetic block) to the end of the
        // if-body, irrespective of whether there is an else statement or not.
        // This allows us to easily branch to the else statement without
        // changing the branching logic or implementing extra case handling for
        // that

        if (isBasicType) {
          common_.condenseSideEffectInstructionToFrameBase();
          conditionCanBeEvaluatedAtCompileTime = stack_.back().getBaseType() == StackType::CONSTANT;
          Stack::iterator const conditionBase{common_.findBaseOfValentBlockBelow(stack_.end())};
          compiler_.backend_.spillAllVariables(conditionBase);

          if (conditionCanBeEvaluatedAtCompileTime) {
            conditionIsAlwaysTrue = conditionBase->data.constUnion.u32 != 0U;
            static_cast<void>(stack_.erase(conditionBase));
          } else {
            // Resolve the condition at the top of the stack and emit an actual
            // comparison so that CPU status flags are set
            branchCond = common_.condenseComparisonBelow(stack_.end());
          }

          Stack::iterator const prevBlockReference{moduleInfo_.fnc.lastBlockReference};
          bool const elseBlockInheritedUnreachable{originalFrameUnreachable || (conditionCanBeEvaluatedAtCompileTime && conditionIsAlwaysTrue)};
          bool const ifBlockInheritedUnreachable{originalFrameUnreachable || (conditionCanBeEvaluatedAtCompileTime && (!conditionIsAlwaysTrue))};
          uint32_t const blockResultsStackOffset{common_.getCurrentMaximumUsedStackFramePosition()};
          // push elseBlock
          Stack::iterator const blockReference{stack_.push(StackElement::block(UnknownIndex, blockResultsStackOffset, prevBlockReference, sigIndex,
                                                                               moduleInfo_.fnc.stackFrameSize, elseBlockInheritedUnreachable))};
          Stack::iterator const ifBlock{stack_.push(StackElement::ifblock(UnknownIndex, blockResultsStackOffset, blockReference, sigIndex,
                                                                          moduleInfo_.fnc.stackFrameSize, ifBlockInheritedUnreachable))};
          moduleInfo_.fnc.lastBlockReference = ifBlock;
          common_.emitBranchDivergePoint(!originalFrameUnreachable, ifBlock);
        } else {
          uint32_t const numBlockParams{moduleInfo_.getNumParamsForSignature(sigIndex)};
          Stack::iterator paramsBase{stack_.end()};
          Stack::iterator conditionElem{};
          common_.condenseSideEffectInstructionBlewValentBlock(numBlockParams);

          if (numBlockParams > 0U) {
            paramsBase = common_.condenseMultipleValentBlocksBelow(common_.findBaseOfValentBlockBelow(stack_.end()), numBlockParams);
          }

          conditionElem = common_.condenseValentBlockBelow(stack_.end());
          branchCond = BC::NEQZ;

          conditionCanBeEvaluatedAtCompileTime = moduleInfo_.getStorage(*conditionElem).type == StorageType::CONSTANT;
          if (conditionCanBeEvaluatedAtCompileTime) {
            conditionIsAlwaysTrue = conditionElem->data.constUnion.u32 != 0U;
            stack_.pop();
          }

          compiler_.backend_.spillAllVariables(paramsBase);

          uint32_t const returnValueStackWidth{common_.getStackReturnValueWidth(sigIndex)};
          uint32_t const blockResultsStackOffset{(returnValueStackWidth == 0U) ? common_.getCurrentMaximumUsedStackFramePosition()
                                                                               : compiler_.backend_.reserveStackFrame(returnValueStackWidth)};
          if (!(originalFrameUnreachable || conditionCanBeEvaluatedAtCompileTime)) {
            static_cast<void>(compiler_.backend_.emitComparison(OPCode::I32_EQZ, conditionElem.unwrap(), nullptr));
            common_.popAndUpdateReference();
          }

          Stack::iterator const prevBlockReference{moduleInfo_.fnc.lastBlockReference};
          bool const elseBlockInheritedUnreachable{originalFrameUnreachable || (conditionCanBeEvaluatedAtCompileTime && conditionIsAlwaysTrue)};
          bool const ifBlockInheritedUnreachable{originalFrameUnreachable || (conditionCanBeEvaluatedAtCompileTime && (!conditionIsAlwaysTrue))};

          Stack::iterator const blockReference{
              stack_.insert(paramsBase, StackElement::block(UnknownIndex, blockResultsStackOffset, prevBlockReference, sigIndex,
                                                            moduleInfo_.fnc.stackFrameSize, elseBlockInheritedUnreachable))};

          Stack::iterator const ifBlock{stack_.push(StackElement::ifblock(UnknownIndex, blockResultsStackOffset, blockReference, sigIndex,
                                                                          moduleInfo_.fnc.stackFrameSize, ifBlockInheritedUnreachable))};
          moduleInfo_.fnc.lastBlockReference = ifBlock;
          common_.emitBranchDivergePoint(!originalFrameUnreachable, ifBlock);

          if (numBlockParams > 0U) {
            Stack::iterator stepIt{paramsBase};
            moduleInfo_.iterateParamsForSignature(sigIndex, FunctionRef<void(MachineType)>([this, &stepIt](MachineType const) {
                                                    assert(!stepIt.isEmpty() && "Nullptr while access parameter element for if with parameters");
                                                    common_.removeReference(stepIt);
                                                    common_.pushAndUpdateReference(*stepIt);
                                                    ++stepIt;
                                                  }));
          }
        }

        // We have a new block, never unreachable
        // Emit a conditional branch (based on the CPU status flags from the
        // previous condition comparison)
        if (!(originalFrameUnreachable || conditionCanBeEvaluatedAtCompileTime)) {
          compiler_.backend_.emitBranch(moduleInfo_.fnc.lastBlockReference.raw(), branchCond, true);
        }
        break;
      }
      case OPCode::ELSE: {
        validationStack_.validateElse();

        Stack::iterator const ifBlock{moduleInfo_.fnc.lastBlockReference};
        assert((!ifBlock.isEmpty() && (ifBlock->type == StackType::IFBLOCK)) && "Else block must terminate an if block");

        Stack::iterator const elseBlock{ifBlock->data.blockInfo.prevBlockReference};
        assert(elseBlock->type == StackType::BLOCK && "Invalid outer if block");
        bool const isReachable{!currentFrameIsUnreachable()};

        uint32_t const blockSigIndex{elseBlock->data.blockInfo.sigIndex};
        uint32_t const numBlockReturnValues{moduleInfo_.getNumReturnValuesForSignature(blockSigIndex)};
        if (isReachable && (numBlockReturnValues > 0U)) {
          Stack::iterator const returnValuesBase{common_.condenseMultipleValentBlocksWithTargetHintBelow(stack_.end(), blockSigIndex)};
          common_.loadReturnValues(returnValuesBase, numBlockReturnValues, elseBlock.raw());
          common_.popReturnValueElems(returnValuesBase, numBlockReturnValues);
        }
        if (!isReachable) {
          cleanCurrentBlockOnUnreachable();
        }

        // `IF A ELSE B` will be convert to `ELSE_BLOCK (IF_BLOCK COND BR_IF(IF_BLOCK) A BR(ELSE_BLOCK) IF_BLOCK_END) B
        // ELSE_BLOCK_END`.
        common_.emitBranchDivergePoint(isReachable, elseBlock); // for BR(ELSE_BLOCK)
        common_.emitBranchMergePoint(false, ifBlock.raw());     // for IF_BLOCK_END

        if (isReachable) {
          // Unconditional branch to end of if (post ELSE)
          compiler_.backend_.emitBranch(elseBlock.raw(), BC::UNCONDITIONAL);
        }

        assert(ifBlock->type == StackType::IFBLOCK && "Wrong block at top");

        // Finalize IFBLOCK so entry if can branch here, end needs to be
        // recorded even if the block is currently unreachable. An earlier
        // branch in that block (when it was still reachable) might branch here
        // and we then pop the synthetic inner if block
        compiler_.backend_.finalizeBlock(ifBlock.raw());
        stack_.pop();

        moduleInfo_.fnc.lastBlockReference = elseBlock;

        for (Stack::iterator it{stack_.last()}; it != elseBlock; --it) {
          common_.addReference(it);
        }
        break;
      }

      case OPCode::END: {
        validationStack_.validateEnd();

        Stack::iterator lastBlock{moduleInfo_.fnc.lastBlockReference};

        if (!lastBlock.isEmpty()) {
          uint32_t const blockSigIndex{lastBlock->data.blockInfo.sigIndex};
          uint32_t const numBlockReturnValues{moduleInfo_.getNumReturnValuesForSignature(blockSigIndex)};
          bool const blockHasReturnElement{numBlockReturnValues > 0U};
          bool originalFrameUnreachable{currentFrameIsUnreachable()};

          if (lastBlock->type == StackType::LOOP) {
            if (!originalFrameUnreachable && blockHasReturnElement) {
              static_cast<void>(common_.condenseMultipleValentBlocksBelow(stack_.end(), numBlockReturnValues));
            }

            compiler_.backend_.finalizeBlock(lastBlock.raw());
            if (originalFrameUnreachable) {
              cleanCurrentBlockOnUnreachable();
            }
            moduleInfo_.fnc.lastBlockReference = lastBlock->data.blockInfo.prevBlockReference;

            popBlockAndPushReturnValues(lastBlock);
            if (blockHasReturnElement && originalFrameUnreachable) {
              pushDummyResultOnUnreachable(blockSigIndex);
            }
            break;
          }

          // Ending if block without else
          if (lastBlock->type == StackType::IFBLOCK) {
            // If there is no else block, it's numParams must equals with numResults ->
            // otherwise non-balanced stack
            uint32_t const numBlockParams{moduleInfo_.getNumParamsForSignature(blockSigIndex)};
            if (numBlockParams != numBlockReturnValues) {
              throw ValidationException(ErrorCode::Type_mismatch_for_if_true_and_false_branches);
            }

            Stack::iterator const outerIfBlock{lastBlock->data.blockInfo.prevBlockReference};
            assert(outerIfBlock->type == StackType::BLOCK && "Invalid outer if block");

            if (!originalFrameUnreachable && (numBlockReturnValues > 0U)) {
              Stack::iterator const returnValuesBase{common_.condenseMultipleValentBlocksWithTargetHintBelow(stack_.end(), blockSigIndex)};
              common_.loadReturnValues(returnValuesBase, numBlockReturnValues, lastBlock.raw());
              common_.popReturnValueElems(returnValuesBase, numBlockReturnValues);
              compiler_.backend_.emitBranch(outerIfBlock.raw(), BC::UNCONDITIONAL);
            }
            common_.emitBranchMergePoint(!originalFrameUnreachable, lastBlock.raw());

            // Finalize the inner block
            compiler_.backend_.finalizeBlock(lastBlock.raw());

            if (originalFrameUnreachable) {
              cleanCurrentBlockOnUnreachable();
            }
            Stack::iterator const ifBlock{lastBlock};
            lastBlock = outerIfBlock;
            moduleInfo_.fnc.lastBlockReference = lastBlock;
            originalFrameUnreachable = currentFrameIsUnreachable();

            popBlockAndPushReturnValues(ifBlock);

            for (Stack::iterator it{stack_.last()}; it != outerIfBlock; --it) {
              common_.addReference(it);
            }
          }

          if (!originalFrameUnreachable && blockHasReturnElement) {
            Stack::iterator const returnValuesBase{common_.condenseMultipleValentBlocksWithTargetHintBelow(stack_.end(), blockSigIndex)};
            common_.loadReturnValues(returnValuesBase, numBlockReturnValues, lastBlock.raw());
            common_.popReturnValueElems(returnValuesBase, numBlockReturnValues);
          }

          common_.emitBranchMergePoint(!originalFrameUnreachable, lastBlock.raw());

          // Finalize the actual block
          compiler_.backend_.finalizeBlock(lastBlock.raw());
          if (originalFrameUnreachable) {
            cleanCurrentBlockOnUnreachable();
          }
          if (lastBlock.isEmpty()) {
            throw ValidationException(ErrorCode::Validation_failed);
          }
          uint32_t const lastBlockResultsStackStartOffset{lastBlock->data.blockInfo.blockResultsStackOffset};
          moduleInfo_.fnc.lastBlockReference = lastBlock->data.blockInfo.prevBlockReference;

          popBlockAndPushReturnValues(lastBlock);

          // push back return element from block, always reachable
          if (blockHasReturnElement) {
            bool const outerFrameIsUnreachable{currentFrameIsUnreachable()};
            NBackend::RegStackTracker tracker{};
            moduleInfo_.iterateResultsForSignature(
                blockSigIndex, FunctionRef<void(MachineType)>([this, outerFrameIsUnreachable, lastBlockResultsStackStartOffset,
                                                               &tracker](MachineType const machineType) {
                  StackElement returnElem{};
                  if (outerFrameIsUnreachable) {
                    returnElem = StackElement::dummyConst(machineType);
                  } else {
                    TReg const returnReg{compiler_.backend_.getREGForReturnValue(machineType, tracker)};
                    if (returnReg != TReg::NONE) {
                      returnElem = StackElement::scratchReg(returnReg, MachineTypeUtil::toStackTypeFlag(machineType));
                    } else {
                      uint32_t const offsetFromSP{lastBlockResultsStackStartOffset - TBackend::offsetInStackReturnValues(tracker, machineType)};
                      returnElem = StackElement::tempResult(machineType, VariableStorage::stackMemory(machineType, offsetFromSP),
                                                            moduleInfo_.getStackMemoryReferencePosition());
                    }
                  }
                  common_.pushAndUpdateReference(returnElem);
                }));
          }

        } else {
          // Function end
          uint32_t const sigIndex{moduleInfo_.getFuncDef(moduleInfo_.fnc.index).sigIndex};
          uint32_t const numReturnValues{moduleInfo_.getNumReturnValuesForSignature(sigIndex)};

          if (currentFrameIsUnreachable()) {
            cleanCurrentBlockOnUnreachable();
          } else {
            if (numReturnValues > 0U) {
              Stack::iterator const returnValuesBase{common_.condenseMultipleValentBlocksWithTargetHintBelow(stack_.end(), sigIndex)};
              common_.loadReturnValues(returnValuesBase, numReturnValues);
              common_.popReturnValueElems(returnValuesBase, numReturnValues);
            }
            compiler_.backend_.emitReturnAndUnwindStack();
          }
          moduleInfo_.fnc.properlyTerminated = true;

          // Break out of the while loop, function is finished
          breakOutOfLoop = true;
        }
        break;
      }

      case OPCode::BR: {
        uint32_t const branchDepth{br_.readLEB128<uint32_t>()};
        validationStack_.validateBranch(instruction, branchDepth);

        Stack::iterator const targetBlockElem{findTargetBlock(branchDepth)};
        bool isLoop{};
        uint32_t sigIndex{};
        if (targetBlockElem.isEmpty()) {
          isLoop = false;
          sigIndex = moduleInfo_.getFuncDef(moduleInfo_.fnc.index).sigIndex;
        } else {
          isLoop = targetBlockElem->type == StackType::LOOP;
          sigIndex = targetBlockElem->data.blockInfo.sigIndex;
        }
        if (!currentFrameIsUnreachable()) {
          uint32_t const numReturnValues{isLoop ? moduleInfo_.getNumParamsForSignature(sigIndex)
                                                : moduleInfo_.getNumReturnValuesForSignature(sigIndex)};

          // If we have an open block, targetBlockElem will point to that block element,
          // otherwise it will be nullptr and the target is thus the function frame itself
          common_.condenseSideEffectInstructionBlewValentBlock(numReturnValues);
          if (numReturnValues > 0U) {
            Stack::iterator const returnValuesBase{common_.condenseMultipleValentBlocksWithTargetHintBelow(stack_.end(), sigIndex, isLoop)};
            common_.loadReturnValues(returnValuesBase, numReturnValues, targetBlockElem.raw());
            common_.popReturnValueElems(returnValuesBase, numReturnValues);
          }

          common_.emitBranchDivergePoint(true, targetBlockElem);
          // Emit an unconditional branch
          compiler_.backend_.emitBranch(targetBlockElem.raw(), BC::UNCONDITIONAL);
        }

        // The code after a BR instruction (which is unconditional!) can never be reached, so set the current frame
        // unreachable
        setCurrentFrameFormallyUnreachable();
        break;
      }
      case OPCode::BR_IF: {
        uint32_t const branchDepth{br_.readLEB128<uint32_t>()};
        validationStack_.validateBranch(instruction, branchDepth);

        if (!currentFrameIsUnreachable()) {
          Stack::iterator const targetBlockElem{findTargetBlock(branchDepth)};
          /// @brief target branch kind
          // coverity[autosar_cpp14_a7_2_2_violation]
          enum class TargetKind : uint8_t { Block, Loop, Return };
          TargetKind const targetKind{targetBlockElem.isEmpty()                    ? TargetKind::Return
                                      : (targetBlockElem->type == StackType::LOOP) ? TargetKind::Loop
                                                                                   : TargetKind::Block};
          uint32_t const sigIndex{(targetKind == TargetKind::Return) ? moduleInfo_.getFuncDef(moduleInfo_.fnc.index).sigIndex
                                                                     : targetBlockElem->data.blockInfo.sigIndex};
          uint32_t const numReturnValues{(targetKind == TargetKind::Loop) ? moduleInfo_.getNumParamsForSignature(sigIndex)
                                                                          : moduleInfo_.getNumReturnValuesForSignature(sigIndex)};

          Stack::iterator returnValuesBase{};
          common_.condenseSideEffectInstructionBlewValentBlock(numReturnValues + 1U);
          if (numReturnValues > 0U) {
            Stack::iterator const baseOfComparison{common_.findBaseOfValentBlockBelow(stack_.end())};
            returnValuesBase = common_.condenseMultipleValentBlocksWithTargetHintBelow(baseOfComparison, sigIndex, targetKind == TargetKind::Loop);
          }

          BC const branchCondition{common_.condenseComparisonBelow(stack_.end())};

          if (numReturnValues > 0U) {
            common_.loadReturnValues(returnValuesBase, numReturnValues, targetBlockElem.raw(), true);
          }

          // there is no need to treat the conditional return as a control flow edge since there are no actually reachable instructions when condition
          // matches.
          if (targetKind == TargetKind::Return) {
            compiler_.backend_.emitBranch(nullptr, branchCondition);
          } else {
            common_.emitBranchDivergePoint(true, targetBlockElem);
            compiler_.backend_.emitBranch(targetBlockElem.raw(), branchCondition);
          }
        }

        break;
      }
      case OPCode::BR_TABLE: {
        // Number of elements in the branch table, excluding a default branch
        // target at the end
        uint32_t const numBranchTargets{br_.readLEB128<uint32_t>()};
        if (numBranchTargets > ImplementationLimits::branchTableLength) {
          throw ImplementationLimitationException(ErrorCode::Too_many_branch_targets_in_br_table);
        }
        bool hasMismatchedReturnType{false};
        uint8_t const *firstBranchTargetResultsPtr{};
        uint32_t firstBranchTargetResultsSize{};
        bool isFirstBranchTarget{true};
        bool const originalFrameUnreachable{currentFrameIsUnreachable()};

        // validate target block of br_table
        uint8_t const *const ptr{br_.getPtr()};
        // coverity[autosar_cpp14_a6_5_1_violation]
        for (uint32_t i{0U}; i < (numBranchTargets + 1U); i++) {
          // Branch depth for this table element
          uint32_t const branchDepth{br_.readLEB128<uint32_t>()};

          // TODO(): move validate logic to ValidationStack
          // temp work around here
          if (i == 0U) {
            validationStack_.validateBranch(instruction, branchDepth);
          }
          // target block element for this table element
          Stack::iterator const targetBlockElem{findTargetBlock(branchDepth)};

          bool isLoop{};
          uint32_t sigIndex{};
          if (targetBlockElem.isEmpty()) {
            isLoop = false;
            sigIndex = moduleInfo_.getFuncDef(moduleInfo_.fnc.index).sigIndex;
          } else {
            isLoop = targetBlockElem->type == StackType::LOOP;
            sigIndex = targetBlockElem->data.blockInfo.sigIndex;
          }
          uint32_t const nextBranchTargetResultsSize{isLoop ? moduleInfo_.getNumParamsForSignature(sigIndex)
                                                            : moduleInfo_.getNumReturnValuesForSignature(sigIndex)};
          uint8_t const *const nextBranchTargetResultsPtr{
              isLoop ? pAddI(pCast<uint8_t *>(moduleInfo_.types()), moduleInfo_.typeOffsets[sigIndex] + 1U)
                     : pAddI(pCast<uint8_t *>(moduleInfo_.types()), moduleInfo_.typeOffsets[sigIndex + 1U] - nextBranchTargetResultsSize)};

          if (isFirstBranchTarget) {
            firstBranchTargetResultsPtr = nextBranchTargetResultsPtr;
            firstBranchTargetResultsSize = nextBranchTargetResultsSize;
            isFirstBranchTarget = false;
          } else {
            bool const isResultsNumMismatched{firstBranchTargetResultsSize != nextBranchTargetResultsSize};
            for (uint32_t index{0U}; index < firstBranchTargetResultsSize; index++) {
              if (readFromPtr<SignatureType>(pAddI(nextBranchTargetResultsPtr, index)) !=
                  readFromPtr<SignatureType>(pAddI(firstBranchTargetResultsPtr, index))) {
                hasMismatchedReturnType = true;
              }
            }
            if (isResultsNumMismatched || (hasMismatchedReturnType && (!originalFrameUnreachable))) {
              throw ValidationException(ErrorCode::br_table_block_return_type_mismatch);
            }
          }
        }

        if (originalFrameUnreachable) {
          break;
        }
        Span<uint8_t const> const span{ptr, static_cast<size_t>(pSubAddr(br_.getPtr(), ptr))};
        BytecodeReader reader{span};
        // coverity[autosar_cpp14_a8_5_2_violation]
        auto const getNextTableBranchDepthLambda = [&reader, this]() -> Stack::iterator {
          uint32_t const branchDepth{reader.readLEB128<uint32_t>()};
          Stack::iterator const targetBlockElem{findTargetBlock(branchDepth)};
          return targetBlockElem;
        };

        // coverity[autosar_cpp14_a5_1_4_violation]
        common_.emitBranchDivergePoint(true, numBranchTargets + 1U, FunctionRef<Stack::iterator()>(getNextTableBranchDepthLambda));

        reader.jumpTo(ptr);
        // coverity[autosar_cpp14_a5_1_4_violation]
        compiler_.backend_.executeTableBranch(numBranchTargets, FunctionRef<Stack::iterator()>(getNextTableBranchDepthLambda));
        setCurrentFrameFormallyUnreachable();
        break;
      }
      case OPCode::RETURN: {
        uint32_t const sigIndex{moduleInfo_.getFuncDef(moduleInfo_.fnc.index).sigIndex};
        validationStack_.validateReturn();
        if (!currentFrameIsUnreachable()) {
          uint32_t const numReturnValues{moduleInfo_.getNumReturnValuesForSignature(sigIndex)};
          common_.condenseSideEffectInstructionBlewValentBlock(numReturnValues);
          if (numReturnValues > 0U) {
            // Load return values and truncate stack
            Stack::iterator const returnValuesBase{common_.condenseMultipleValentBlocksWithTargetHintBelow(stack_.end(), sigIndex)};
            common_.loadReturnValues(returnValuesBase, numReturnValues);
            common_.popReturnValueElems(returnValuesBase, numReturnValues);
          }
          compiler_.backend_.emitReturnAndUnwindStack(true); // Emit actual return instruction and unwind the stack
        }

        setCurrentFrameFormallyUnreachable();
        break;
      }
      case OPCode::CALL: {
        // The called function index (not the table index) is given as an immediate to this instruction
        uint32_t const calledFunctionIndex{br_.readLEB128<uint32_t>()};

        if (calledFunctionIndex >= moduleInfo_.numTotalFunctions) {
          throw ValidationException(ErrorCode::Function_index_out_of_range);
        }
        uint32_t const sigIndex{moduleInfo_.getFncSigIndex(calledFunctionIndex)};
        validationStack_.validateCall(sigIndex);

        if (!currentFrameIsUnreachable()) {
          uint32_t const numParamsCallee{moduleInfo_.getNumParamsForSignature(sigIndex)};
          common_.condenseSideEffectInstructionBlewValentBlock(numParamsCallee);
          // Check whether this call a builtin function
#if BUILTIN_FUNCTIONS
          if (moduleInfo_.functionIsBuiltin(calledFunctionIndex)) {
            ModuleInfo::ImpFuncDef const impFuncDef{moduleInfo_.getImpFuncDef(calledFunctionIndex)};
            compiler_.backend_.execBuiltinFncCall(impFuncDef.builtinFunction);
            break;
          }
#endif
          compiler_.backend_.execDirectFncCall(calledFunctionIndex);
        }

        break;
      }
      case OPCode::CALL_INDIRECT: {
        // The index of the function type/signature is given as an immediate to this instruction
        uint32_t const sigIndex{reduceTypeIndex(br_.readLEB128<uint32_t>())};
        validationStack_.validateLastNumberType(MachineType::I32, true); // func index in table
        validationStack_.validateCall(sigIndex);
        // Only table index 0 is supported in the MVP of Wasm
        uint32_t const tableIndex{br_.readLEB128<uint32_t>()};
        if ((!moduleInfo_.hasTable) || (tableIndex != 0U)) {
          throw ValidationException(ErrorCode::Table_not_found);
        }

        if (!currentFrameIsUnreachable()) {
          uint32_t const numParamsCallee{moduleInfo_.getNumParamsForSignature(sigIndex)};
          common_.condenseSideEffectInstructionBlewValentBlock(numParamsCallee);
          compiler_.backend_.execIndirectWasmCall(sigIndex, tableIndex);
        }
        break;
      }

      case OPCode::DROP: {
        validationStack_.drop();

        if (!currentFrameIsUnreachable()) {
          common_.condenseCurrentValentBlockIfSideEffect();
          common_.dropValentBlock();
        }

        break;
      }
      case OPCode::SELECT: {
        validationStack_.validateSelect();

        if (!currentFrameIsUnreachable()) {
          // We handle the SELECT instruction as a regular deferred action, the evaluation mechanisms will be able to
          // handle this
          Stack::iterator const iterator{common_.pushDeferredAction(StackElement::action(instruction))};
          static_cast<void>(iterator);
#if ENABLE_EXTENSIONS
          if (compiler_.dwarfGenerator_ != nullptr) {
            compiler_.dwarfGenerator_->registerPendingDeferAction(iterator.unwrap(), static_cast<uint32_t>(bytecodePosition));
          }
#endif
          if (compiler_.getDebugMode()) {
            static_cast<void>(common_.condenseValentBlockBelow(stack_.end()));
          }
        }
        break;
      }
      case OPCode::SELECT_T: {
        throw FeatureNotSupportedException(ErrorCode::Reference_type_feature_not_implemented);
      }

      case OPCode::LOCAL_GET: {
        uint32_t const localIdx{br_.readLEB128<uint32_t>()};
        if (localIdx >= moduleInfo_.fnc.numLocals) {
          throw ValidationException(ErrorCode::Local_out_of_range);
        }
        validationStack_.pushNumberVariable(moduleInfo_.localDefs[localIdx].type);
        if (currentFrameIsUnreachable()) {
          break;
        }
        // Recover local to reg for performance reasons
        common_.recoverLocalToReg(localIdx, !currentFrameIsUnreachable());
        // Simply record the local (by reference) on the stack
        if (moduleInfo_.localDefs[localIdx].currentStorageType == StorageType::CONSTANT) {
          switch (moduleInfo_.localDefs[localIdx].type) {
          case MachineType::F64:
            common_.pushAndUpdateReference(StackElement::f64Const(0.0));
            break;
          case MachineType::F32:
            common_.pushAndUpdateReference(StackElement::f32Const(0.0F));
            break;
          case MachineType::I64:
            common_.pushAndUpdateReference(StackElement::i64Const(0U));
            break;
          case MachineType::I32:
            common_.pushAndUpdateReference(StackElement::i32Const(0U));
            break;
          default:
            UNREACHABLE(break, "Invalid local type");
          }
        } else {
          common_.pushAndUpdateReference(StackElement::local(localIdx));
        }
        break;
      }
      case OPCode::LOCAL_TEE:
      case OPCode::LOCAL_SET: {
        uint32_t const localIdx{br_.readLEB128<uint32_t>()};
        if (localIdx >= moduleInfo_.fnc.numLocals) {
          throw ValidationException(ErrorCode::Local_out_of_range);
        }
        StackElement const targetElem{StackElement::local(localIdx)};

        validationStack_.validateLastNumberType(moduleInfo_.localDefs[localIdx].type, instruction == OPCode::LOCAL_SET);

        if (!currentFrameIsUnreachable()) {
          common_.condenseSideEffectInstructionBlewValentBlock(1U);
          common_.prepareLocalForSetValue(localIdx);

          // Condense the topmost valent block (possibly consisting of multiple StackElements) and tell the backend to
          // enforce putting the result into the targetElem representing a reference to that local variable
          static_cast<void>(common_.condenseValentBlockBelow(stack_.end(), &targetElem));

          // Since LOCAL_SET consumes the element and condenseValentBlock will put the (enforced) target onto the stack,
          // we need to pop it
          if (instruction == OPCode::LOCAL_SET) {
            common_.popAndUpdateReference();
          }
        }
        break;
      }

      case OPCode::GLOBAL_GET: {
        uint32_t const globalIdx{br_.readLEB128<uint32_t>()};
        if (globalIdx >= moduleInfo_.numNonImportedGlobals) {
          throw ValidationException(ErrorCode::Global_out_of_range);
        }
        validationStack_.pushNumberVariable(moduleInfo_.globals[globalIdx].type);
        if (currentFrameIsUnreachable()) {
          break;
        }

        // Retrieve the definition for this global variable
        // This includes the type, at which offset in link data it is stored, the initial value and whether it is
        // mutable
        ModuleInfo::GlobalDef const &globalDef{moduleInfo_.globals[globalIdx]};

        if (globalDef.isMutable) {
          // Record the (mutable) global variable (by reference) on the stack
          common_.pushAndUpdateReference(StackElement::global(globalIdx));
        } else {
          // Do not push constant globals als global Variable to the stack
          StackElement constElement{};
          constElement.type = MachineTypeUtil::toStackTypeFlag(globalDef.type) | StackType::CONSTANT;
          constElement.data.constUnion = globalDef.initialValue;
          static_cast<void>(common_.pushOperandsToStack(constElement));
        }
        break;
      }
      case OPCode::GLOBAL_SET: {
        uint32_t const globalIdx{br_.readLEB128<uint32_t>()};
        if (globalIdx >= moduleInfo_.numNonImportedGlobals) {
          throw ValidationException(ErrorCode::Global_out_of_range);
        }

        validationStack_.validateLastNumberType(moduleInfo_.globals[globalIdx].type, true);

        // Retrieve the definition for this global variable
        // This includes the type, at which offset in link data it is stored, the initial value and whether it is
        // mutable
        ModuleInfo::GlobalDef const &globalDef{moduleInfo_.globals[globalIdx]};

        if (!globalDef.isMutable) {
          throw ValidationException(ErrorCode::Cannot_set_immutable_global);
        }

        if (!currentFrameIsUnreachable()) {
          // here can't use condenseSideEffectInstructionBlewValentBlock because it make change the value in global as target hint.
          common_.condenseSideEffectInstructionToFrameBase();
          StackElement const targetElem{StackElement::global(globalIdx)};
          // Condense the topmost valent block (possibly consisting of multiple StackElements) and tell the backend to
          // enforce putting the result into the targetElem representing a reference to that global variable
          static_cast<void>(common_.condenseValentBlockBelow(stack_.end(), &targetElem));

          // Since GLOBAL_SET consumes the element and condenseValentBlock will put the (enforced) target onto the
          // stack, we need to pop it
          common_.popAndUpdateReference();
        }
        break;
      }

      case OPCode::TABLE_GET:
      case OPCode::TABLE_SET:
      case OPCode::TABLE_INIT:
      case OPCode::ELEMENT_DROP:
      case OPCode::TABLE_COPY:
      case OPCode::TABLE_GROW:
      case OPCode::TABLE_SIZE:
      case OPCode::TABLE_FILL: {
        throw FeatureNotSupportedException(ErrorCode::Reference_type_feature_not_implemented);
      }

      case OPCode::I32_LOAD:
      case OPCode::I64_LOAD:
      case OPCode::F32_LOAD:
      case OPCode::F64_LOAD:
      case OPCode::I32_LOAD8_S:
      case OPCode::I32_LOAD8_U:
      case OPCode::I32_LOAD16_S:
      case OPCode::I32_LOAD16_U:
      case OPCode::I64_LOAD8_S:
      case OPCode::I64_LOAD8_U:
      case OPCode::I64_LOAD16_S:
      case OPCode::I64_LOAD16_U:
      case OPCode::I64_LOAD32_S:
      case OPCode::I64_LOAD32_U: {
        if (!moduleInfo_.hasMemory) {
          throw ValidationException(ErrorCode::Undefined_memory_referenced);
        }

        validationStack_.validateLastNumberType(MachineType::I32, true);
        validationStack_.pushNumberVariable(getLoadResultType(instruction));

        // Maximally allowed alignment for each store operation, as power of two, corresponding to (non-extended)
        // operand size of the instruction
        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto maxAlignmentPow2 = make_array(2_U8, 3_U8, 2_U8, 3_U8, 0_U8, 0_U8, 1_U8, 1_U8, 0_U8, 0_U8, 1_U8, 1_U8, 2_U8, 2_U8);
        uint32_t const alignment{br_.readLEB128<uint32_t>()};
        if (alignment > maxAlignmentPow2[static_cast<uint32_t>(instruction) - static_cast<uint32_t>(OPCode::I32_LOAD)]) {
          throw ValidationException(ErrorCode::Alignment_out_of_range);
        }

        // The resulting address is encoded as an unsigned i32 variable plus an unsigned i32 immediate offset (encoded
        // in the instruction)
        uint32_t const offset{br_.readLEB128<uint32_t>()};

        if (!currentFrameIsUnreachable()) {
          Stack::iterator const iterator{common_.pushDeferredAction(StackElement::action(instruction, 1U, offset))};
          static_cast<void>(iterator);
          if (compiler_.getDebugMode()) {
            static_cast<void>(common_.condenseValentBlockBelow(stack_.end()));
          }
#if ENABLE_EXTENSIONS
          if (compiler_.dwarfGenerator_ != nullptr) {
            compiler_.dwarfGenerator_->registerPendingDeferAction(iterator.unwrap(), static_cast<uint32_t>(bytecodePosition));
          }
#endif
        }
        break;
      }

      case OPCode::I32_STORE:
      case OPCode::I64_STORE:
      case OPCode::F32_STORE:
      case OPCode::F64_STORE:
      case OPCode::I32_STORE8:
      case OPCode::I32_STORE16:
      case OPCode::I64_STORE8:
      case OPCode::I64_STORE16:
      case OPCode::I64_STORE32: {
        if (!moduleInfo_.hasMemory) {
          throw ValidationException(ErrorCode::Undefined_memory_referenced);
        }

        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto inputType = make_array(MachineType::I32, MachineType::I64, MachineType::F32, MachineType::F64, MachineType::I32,
                                              MachineType::I32, MachineType::I64, MachineType::I64, MachineType::I64);
        validationStack_.validateLastNumberType(inputType[static_cast<uint32_t>(instruction) - static_cast<uint32_t>(OPCode::I32_STORE)], true);
        validationStack_.validateLastNumberType(MachineType::I32, true);

        // Maximally allowed alignment for each store operation, as power of two, corresponding to (the reduced) operand
        // size of the instruction
        // coverity[autosar_cpp14_a8_5_2_violation]
        constexpr auto maxAlignmentPow2 = make_array(2_U8, 3_U8, 2_U8, 3_U8, 0_U8, 1_U8, 0_U8, 1_U8, 2_U8);
        uint32_t const alignment{br_.readLEB128<uint32_t>()};
        if (alignment > maxAlignmentPow2[static_cast<uint32_t>(instruction) - static_cast<uint32_t>(OPCode::I32_STORE)]) {
          throw ValidationException(ErrorCode::Alignment_out_of_range);
        }

        // The resulting address is encoded as an unsigned i32 variable plus an unsigned i32 immediate offset (encoded
        // in the instruction)
        uint32_t const offset{br_.readLEB128<uint32_t>()};
        if (!currentFrameIsUnreachable()) {
          common_.condenseSideEffectInstructionBlewValentBlock(2U);
          compiler_.backend_.executeLinearMemoryStore(instruction, offset);
        }
        break;
      }

      case OPCode::MEMORY_SIZE: {
        if (!moduleInfo_.hasMemory) {
          throw ValidationException(ErrorCode::Undefined_memory_referenced);
        }
        validationStack_.pushNumberVariable(MachineType::I32);

        // The spec defines a reserved "index" not doing anything for now here
        uint8_t const reservedByte{br_.readByte<uint8_t>()};
        if (reservedByte != 0U) {
          throw ValidationException(ErrorCode::memory_size_reserved_value_must_be_a_zero_byte);
        }
        if (!currentFrameIsUnreachable()) {
          compiler_.backend_.executeGetMemSize();
        }
        break;
      }

      case OPCode::MEMORY_GROW: {
        if (!moduleInfo_.hasMemory) {
          throw ValidationException(ErrorCode::Undefined_memory_referenced);
        }
        validationStack_.validateLastNumberType(MachineType::I32, true);
        validationStack_.pushNumberVariable(MachineType::I32);

        // The spec defines a reserved "index" byte not doing anything for now here
        uint8_t const reservedByte{br_.readByte<uint8_t>()};
        if (reservedByte != 0U) {
          throw ValidationException(ErrorCode::memory_grow_reserved_value_must_be_a_zero_byte);
        }

        if (!currentFrameIsUnreachable()) {
          common_.condenseSideEffectInstructionBlewValentBlock(1U);
          compiler_.backend_.executeMemGrow();
        }

        break;
      }

      case OPCode::MEMORY_INIT:
      case OPCode::DATA_DROP:
        throw FeatureNotSupportedException(ErrorCode::Bulk_memory_operations_feature_not_implemented);

      case OPCode::MEMORY_COPY: {
        if ((br_.readByte<uint8_t>() != 0x00U)) {
          throw ValidationException(ErrorCode::Unknown_instruction);
        }
        if ((br_.readByte<uint8_t>() != 0x00U)) {
          throw ValidationException(ErrorCode::Unknown_instruction);
        }
        // The memory C.mems[0] must be defined in the context.
        if (!moduleInfo_.hasMemory) {
          throw ValidationException(ErrorCode::Undefined_memory_referenced);
        }

        validationStack_.validateLastNumberType(MachineType::I32, true);
        validationStack_.validateLastNumberType(MachineType::I32, true);
        validationStack_.validateLastNumberType(MachineType::I32, true);

        if (!currentFrameIsUnreachable()) {
          common_.condenseSideEffectInstructionBlewValentBlock(3U);
          // Then the instruction is valid with type [i32 i32 i32] -> [].
          Stack::iterator const size{common_.condenseValentBlockBelow(stack_.end())};
          Stack::iterator const src{common_.condenseValentBlockBelow(size)};
          Stack::iterator const dst{common_.condenseValentBlockBelow(src)};

          compiler_.backend_.executeLinearMemoryCopy(dst, src, size);
        }

        break;
      }
      case OPCode::MEMORY_FILL: {
        if ((br_.readByte<uint8_t>() != 0x00U)) {
          throw ValidationException(ErrorCode::Unknown_instruction);
        }
        // The memory C.mems[0] must be defined in the context.
        if (!moduleInfo_.hasMemory) {
          throw ValidationException(ErrorCode::Undefined_memory_referenced);
        }

        validationStack_.validateLastNumberType(MachineType::I32, true);
        validationStack_.validateLastNumberType(MachineType::I32, true);
        validationStack_.validateLastNumberType(MachineType::I32, true);

        if (!currentFrameIsUnreachable()) {
          common_.condenseSideEffectInstructionBlewValentBlock(3U);
          // Then the instruction is valid with type [i32 i32 i32] -> [].
          Stack::iterator const size{common_.condenseValentBlockBelow(stack_.end())};
          Stack::iterator const value{common_.condenseValentBlockBelow(size)};
          Stack::iterator const dst{common_.condenseValentBlockBelow(value)};

          compiler_.backend_.executeLinearMemoryFill(dst, value, size);
        }

        break;
      }

      case OPCode::I32_CONST: {
        validationStack_.pushNumberVariable(MachineType::I32);
        // Read LEB128-encoded 4-byte integer and push to stack
        uint32_t const value{bit_cast<uint32_t>(br_.readLEB128<int32_t>())};
        if (!currentFrameIsUnreachable()) {
          static_cast<void>(compiler_.common_.pushOperandsToStack(StackElement::i32Const(value)));
        }
        break;
      }
      case OPCode::I64_CONST: {
        validationStack_.pushNumberVariable(MachineType::I64);
        // Read LEB128-encoded 8-byte integer and push to stack
        uint64_t const value{bit_cast<uint64_t>(br_.readLEB128<int64_t>())};
        if (!currentFrameIsUnreachable()) {
          static_cast<void>(common_.pushOperandsToStack(StackElement::i64Const(value)));
        }
        break;
      }
      case OPCode::F32_CONST: {
        validationStack_.pushNumberVariable(MachineType::F32);
        // Read float (4-byte) value and push to stack
        float const value{bit_cast<float>(br_.readLEU32())};
        if (!currentFrameIsUnreachable()) {
          static_cast<void>(common_.pushOperandsToStack(StackElement::f32Const(value)));
        }
        break;
      }
      case OPCode::F64_CONST: {
        validationStack_.pushNumberVariable(MachineType::F64);
        // Read double (8-byte) value and push to stack
        double const value{bit_cast<double>(br_.readLEU64())};
        if (!currentFrameIsUnreachable()) {
          static_cast<void>(common_.pushOperandsToStack(StackElement::f64Const(value)));
        }
        break;
      }

      case OPCode::I32_EQZ:
      case OPCode::I32_EQ:
      case OPCode::I32_NE:
      case OPCode::I32_LT_S:
      case OPCode::I32_LT_U:
      case OPCode::I32_GT_S:
      case OPCode::I32_GT_U:
      case OPCode::I32_LE_S:
      case OPCode::I32_LE_U:
      case OPCode::I32_GE_S:
      case OPCode::I32_GE_U:
      case OPCode::I64_EQZ:
      case OPCode::I64_EQ:
      case OPCode::I64_NE:
      case OPCode::I64_LT_S:
      case OPCode::I64_LT_U:
      case OPCode::I64_GT_S:
      case OPCode::I64_GT_U:
      case OPCode::I64_LE_S:
      case OPCode::I64_LE_U:
      case OPCode::I64_GE_S:
      case OPCode::I64_GE_U:
      case OPCode::F32_EQ:
      case OPCode::F32_NE:
      case OPCode::F32_LT:
      case OPCode::F32_GT:
      case OPCode::F32_LE:
      case OPCode::F32_GE:
      case OPCode::F64_EQ:
      case OPCode::F64_NE:
      case OPCode::F64_LT:
      case OPCode::F64_GT:
      case OPCode::F64_LE:
      case OPCode::F64_GE:
      case OPCode::I32_CLZ:
      case OPCode::I32_CTZ:
      case OPCode::I32_POPCNT:
      case OPCode::I32_ADD:
      case OPCode::I32_SUB:
      case OPCode::I32_MUL:
      case OPCode::I32_DIV_S:
      case OPCode::I32_DIV_U:
      case OPCode::I32_REM_S:
      case OPCode::I32_REM_U:
      case OPCode::I32_AND:
      case OPCode::I32_OR:
      case OPCode::I32_XOR:
      case OPCode::I32_SHL:
      case OPCode::I32_SHR_S:
      case OPCode::I32_SHR_U:
      case OPCode::I32_ROTL:
      case OPCode::I32_ROTR:
      case OPCode::I64_CLZ:
      case OPCode::I64_CTZ:
      case OPCode::I64_POPCNT:
      case OPCode::I64_ADD:
      case OPCode::I64_SUB:
      case OPCode::I64_MUL:
      case OPCode::I64_DIV_S:
      case OPCode::I64_DIV_U:
      case OPCode::I64_REM_S:
      case OPCode::I64_REM_U:
      case OPCode::I64_AND:
      case OPCode::I64_OR:
      case OPCode::I64_XOR:
      case OPCode::I64_SHL:
      case OPCode::I64_SHR_S:
      case OPCode::I64_SHR_U:
      case OPCode::I64_ROTL:
      case OPCode::I64_ROTR:
      case OPCode::F32_ABS:
      case OPCode::F32_NEG:
      case OPCode::F32_CEIL:
      case OPCode::F32_FLOOR:
      case OPCode::F32_TRUNC:
      case OPCode::F32_NEAREST:
      case OPCode::F32_SQRT:
      case OPCode::F32_ADD:
      case OPCode::F32_SUB:
      case OPCode::F32_MUL:
      case OPCode::F32_DIV:
      case OPCode::F32_MIN:
      case OPCode::F32_MAX:
      case OPCode::F32_COPYSIGN:
      case OPCode::F64_ABS:
      case OPCode::F64_NEG:
      case OPCode::F64_CEIL:
      case OPCode::F64_FLOOR:
      case OPCode::F64_TRUNC:
      case OPCode::F64_NEAREST:
      case OPCode::F64_SQRT:
      case OPCode::F64_ADD:
      case OPCode::F64_SUB:
      case OPCode::F64_MUL:
      case OPCode::F64_DIV:
      case OPCode::F64_MIN:
      case OPCode::F64_MAX:
      case OPCode::F64_COPYSIGN:
      case OPCode::I32_WRAP_I64:
      case OPCode::I32_TRUNC_F32_S:
      case OPCode::I32_TRUNC_F32_U:
      case OPCode::I32_TRUNC_F64_S:
      case OPCode::I32_TRUNC_F64_U:
      case OPCode::I64_EXTEND_I32_S:
      case OPCode::I64_EXTEND_I32_U:
      case OPCode::I64_TRUNC_F32_S:
      case OPCode::I64_TRUNC_F32_U:
      case OPCode::I64_TRUNC_F64_S:
      case OPCode::I64_TRUNC_F64_U:
      case OPCode::F32_CONVERT_I32_S:
      case OPCode::F32_CONVERT_I32_U:
      case OPCode::F32_CONVERT_I64_S:
      case OPCode::F32_CONVERT_I64_U:
      case OPCode::F32_DEMOTE_F64:
      case OPCode::F64_CONVERT_I32_S:
      case OPCode::F64_CONVERT_I32_U:
      case OPCode::F64_CONVERT_I64_S:
      case OPCode::F64_CONVERT_I64_U:
      case OPCode::F64_PROMOTE_F32:
      case OPCode::I32_REINTERPRET_F32:
      case OPCode::I64_REINTERPRET_F64:
      case OPCode::F32_REINTERPRET_I32:
      case OPCode::F64_REINTERPRET_I64:
      case OPCode::I32_EXTEND8_S:
      case OPCode::I32_EXTEND16_S:
      case OPCode::I64_EXTEND8_S:
      case OPCode::I64_EXTEND16_S:
      case OPCode::I64_EXTEND32_S: {
        validationStack_.validateArithmeticElement(instruction);

        if (currentFrameIsUnreachable()) {
          break;
        }
        bool const canTrap{Common::opcodeCanTrap(instruction)};
        uint16_t const sideEffect{canTrap ? static_cast<uint16_t>(1U) : static_cast<uint16_t>(0U)};
        StackElement const action{StackElement::action(instruction, sideEffect, 0U)};
        Stack::iterator const iterator{common_.pushDeferredAction(action)};
        static_cast<void>(iterator);
#if ENABLE_EXTENSIONS
        if (compiler_.dwarfGenerator_ != nullptr) {
          compiler_.dwarfGenerator_->registerPendingDeferAction(iterator.unwrap(), static_cast<uint32_t>(bytecodePosition));
        }
#endif
        if (compiler_.getDebugMode()) {
          static_cast<void>(common_.condenseValentBlockBelow(stack_.end()));
        }
        break;
      }

      case OPCode::I32_TRUNC_SAT_F32_S:
      case OPCode::I32_TRUNC_SAT_F32_U:
      case OPCode::I32_TRUNC_SAT_F64_S:
      case OPCode::I32_TRUNC_SAT_F64_U:
      case OPCode::I64_TRUNC_SAT_F32_S:
      case OPCode::I64_TRUNC_SAT_F32_U:
      case OPCode::I64_TRUNC_SAT_F64_S:
      case OPCode::I64_TRUNC_SAT_F64_U: {
        throw FeatureNotSupportedException(ErrorCode::Non_trapping_float_to_int_conversions_not_implemented);
      }
      case OPCode::REF_NULL:
      case OPCode::REF_IS_NULL:
      case OPCode::REF_FUNC: {
        throw FeatureNotSupportedException(ErrorCode::Reference_type_feature_not_implemented);
      }

      default: {
        // default is triggered when an unknown instruction or an instruction that is not (yet) implemented, is
        // encountered
        throw ValidationException(ErrorCode::Unknown_instruction);
      }
      }

      if (compiler_.getDebugMode()) {
        writeDebugMapInstructionRecordIfNeeded();
      }
#if ENABLE_EXTENSIONS
      if (compiler_.dwarfGenerator_ != nullptr) {
        compiler_.dwarfGenerator_->finishOp();
      }
#endif
    }

    // Function has ended
    // Check if it has gone through an "end" instruction while the correct number of stack elements corresponding to the
    // number of return values
    if (!moduleInfo_.fnc.properlyTerminated) {
      throw ValidationException(ErrorCode::Function_was_not_terminated_properly);
    }

    // Check that no block is referenced anymore
    // The last "end" instruction of a function implies that no block, loop or ifblock is active anymore
    assert(moduleInfo_.fnc.lastBlockReference.isEmpty() && "There is still a block referenced");

    // Confirm that the function body size matches
    if (br_.getPtr() != pAddI(functionPosAfterSize, functionBodySize)) {
      throw ValidationException(ErrorCode::Function_size_mismatch);
    }

    // Check whether all indices have been cleared (DEBUG only)
    for (uint32_t i{0U}; i < numVariableIndices; i++) {
      assert(moduleInfo_.referencesToLastOccurrenceOnStack[i].isEmpty() && "Variable index not cleared at end of function");
    }

#if ENABLE_EXTENSIONS
    if (dwarfGenerator != nullptr) {
      dwarfGenerator->finishOp();
      dwarfGenerator->finishFunction(compiler_.output_.size());
    }
#endif

    // Use padding to align the next section to 4 bytes again so we can guarantee this alignment irrespective of the
    // underlying ISA
    uint32_t const bytesForAlignment{deltaToNextPow2(compiler_.output_.size(), 2U)};
    compiler_.output_.step(bytesForAlignment);                                                                   // OPBVF1
    uint32_t const functionJitSize{compiler_.output_.size() - moduleInfo_.wasmFncBodyBinaryPositions[fncIndex]}; //
    compiler_.output_.write<uint32_t>(functionJitSize);                                                          // OPBVF2

#if ENABLE_EXTENSIONS
    if (compiler_.getAnalytics() != nullptr) {
      compiler_.getAnalytics()->updateMaxFunctionJitSize(functionJitSize);
    }
#endif

    // Patch debug information machine code length
    if (compiler_.getDebugMode()) {
      patchDebugMapRef(debugMapRef);
    }

    moduleInfo_.numFunctionBodiesProduced++;
  }
}

void Frontend::parseDataCountSection() {
  moduleInfo_.numDataSegments = br_.readLEB128<uint32_t>();
}

// Data section, if present
void Frontend::parseDataSection() {
  uint32_t const numDataSegments{br_.readLEB128<uint32_t>()};
  if (moduleInfo_.numDataSegments == UINT32_MAX) {
    moduleInfo_.numDataSegments = numDataSegments;
  } else {
    if (moduleInfo_.numDataSegments != numDataSegments) {
      throw ValidationException(ErrorCode::Data_count_and_data_section_have_inconsistent_lengths);
    }
  }
  for (uint32_t i{0U}; i < numDataSegments; i++) {
    enum class DataMode : uint32_t { Active, Passive, ActiveInNonDefaultMemory };
    // coverity[autosar_cpp14_a7_2_1_violation]
    DataMode const mode{static_cast<DataMode>(br_.readLEB128<uint32_t>())};
    if (mode != DataMode::Active) {
      throw FeatureNotSupportedException(ErrorCode::Passive_mode_data_segments_not_implemented);
    }
    if (!moduleInfo_.hasMemory) {
      throw ValidationException(ErrorCode::Memory_index_out_of_bounds);
    }

    OPCode instruction{parseOpCode(br_)};
    uint32_t offset;
    if ((instruction >= OPCode::I32_CONST) && (instruction <= OPCode::F64_CONST)) {
      if (instruction != OPCode::I32_CONST) {
        throw ValidationException(ErrorCode::Constant_expression_offset_has_to_be_of_type_i32);
      }
      offset = br_.readLEB128<uint32_t>();
      instruction = parseOpCode(br_);
      if (instruction != OPCode::END) {
        throw ValidationException(ErrorCode::Malformed_constant_expression_offset);
      }
    } else if (instruction == OPCode::GLOBAL_GET) {
      throw FeatureNotSupportedException(ErrorCode::Imported_globals_not_supported);
    } else {
      throw ValidationException(ErrorCode::Malformed_constant_expression_offset);
    }

    uint32_t const segmentSize{br_.readLEB128<uint32_t>()};

    // 65536 bytes is the Wasm page size
    uint64_t const endOffset{static_cast<uint64_t>(offset) + segmentSize};
    uint64_t const initialMemSizeBytes{static_cast<uint64_t>(moduleInfo_.memoryInitialSize) * 65536U};
    if ((endOffset > initialMemSizeBytes) || (offset > (UINT32_MAX - segmentSize))) {
      throw ValidationException(ErrorCode::Data_segment_out_of_initial_bounds);
    }
    uint8_t const *const data{br_.getPtr()};
    static_cast<void>(data);
    br_.step(segmentSize);

    // We pad to align to 4B, then write the raw initial data.
    // After that a  u32 representing the actual (non-padded) length of the
    // segment and a u32 representing the start offset of the segment
    if (segmentSize > 0U) {
      // Start to write metadata after the function bodies
      uint32_t const stepWidth{roundUpToPow2(segmentSize, 2U)};
      compiler_.output_.step(stepWidth);
      // OPBVLM1
      static_cast<void>(std::memcpy(pSubI(compiler_.output_.ptr(), stepWidth), data, static_cast<size_t>(segmentSize))); // OPBVLM0
    }
    compiler_.output_.write<uint32_t>(segmentSize); // OPBVLM2
    compiler_.output_.write<uint32_t>(offset);      // OPBVLM3
  }
  // Number of data segments (OPBVLM4) will be written to the binary outside of
  // this section, because this needs to happen irrespectively of whether this
  // section is present or not
}

// Will serialize the Wasm start function section, having a wrapper for the
// start function and signature for a signature check via the standard
// RawModuleFunction without extra case handling
void Frontend::serializeStartFunctionSection() {
  uint32_t const sectionStartSize{compiler_.output_.size()};
  if (moduleInfo_.hasStartFunction) {
    writePaddedBinaryBlob(FunctionRef<void()>([this]() {
      compiler_.backend_.emitFunctionEntryPoint(moduleInfo_.startFunctionIndex);
    })); // OPBVF0

    // Validate start function signature
    uint32_t const sigIndex{moduleInfo_.getFncSigIndex(moduleInfo_.startFunctionIndex)};
    uint32_t const typeOffset{moduleInfo_.typeOffsets[sigIndex]};
    uint32_t const nextTypeOffset{moduleInfo_.typeOffsets[sigIndex + 1U]};
    uint32_t const signatureLength{nextTypeOffset - typeOffset};
    char const *const signature{pAddI(pCast<char const *>(moduleInfo_.types()), typeOffset)};
    assert(signatureLength == 2 && "Start function not nullary");

    uint32_t const stepWidth{roundUpToPow2(signatureLength, 2U)};
    compiler_.output_.step(stepWidth); // OPBVSF4 (Padding)
    // OPBVSF3
    // NOLINTNEXTLINE(clang-analyzer-core.NonNullParamChecker,clang-analyzer-unix.cstring.NullArg,-warnings-as-errors)
    static_cast<void>(std::memcpy(pSubI(compiler_.output_.ptr(), stepWidth), signature, static_cast<size_t>(signatureLength)));
    compiler_.output_.write<uint32_t>(signatureLength); // OPBVSF5
  }
  uint32_t const sectionSize{compiler_.output_.size() - sectionStartSize};
  compiler_.output_.write<uint32_t>(sectionSize); // OPBVSF6
}

// Will serialize the Wasm globals section, that will contain definitions for
// non-imported mutable Wasm globals
void Frontend::serializeWasmGlobalsBinarySection() {
  uint32_t const sectionStartSize{compiler_.output_.size()};
  uint32_t writtenGlobals{0U};
  for (uint32_t i{0U}; i < moduleInfo_.numNonImportedGlobals; i++) {
    ModuleInfo::GlobalDef const &globalDef{moduleInfo_.globals[i]};
    // Non-mutable globals will be inlined anyway
    if (!globalDef.isMutable) {
      continue;
    }

    // OPBVNG0
    switch (globalDef.type) {
    case MachineType::I32:
      compiler_.output_.write<uint32_t>(globalDef.initialValue.u32);
      break;
    case MachineType::I64:
      compiler_.output_.write<uint64_t>(globalDef.initialValue.u64);
      break;
    case MachineType::F32:
      compiler_.output_.write<float>(globalDef.initialValue.f32);
      break;
    case MachineType::F64:
      compiler_.output_.write<double>(globalDef.initialValue.f64);
      break;
    case MachineType::INVALID:
    // GCOVR_EXCL_START
    default: {
      assert(false);
      break;
    }
      // GCOVR_EXCL_STOP
    }

    compiler_.output_.write<uint32_t>(globalDef.linkDataOffset); // OPBVNG1
    compiler_.output_.write<MachineType>(globalDef.type);        // OPBVNG2
    compiler_.output_.step(3U);                                  // OPBVNG3 (Padding)
    writtenGlobals++;
  }
  compiler_.output_.write<uint32_t>(writtenGlobals);                       // OPBVNG4
  uint32_t const sectionSize{compiler_.output_.size() - sectionStartSize}; //
  compiler_.output_.write<uint32_t>(sectionSize);                          // OPBVNG5
}

// Serialize a section for dynamically imported functions. Contains their
// function signature, import names and the offset of the pointer in
// link data so that the runtime can successfully link functions
void Frontend::serializeDynamicFunctionImportBinarySection() {
  uint32_t const sectionStartSize{compiler_.output_.size()};
  uint32_t numDynamicImports{0U};
  for (uint32_t i{0U}; i < moduleInfo_.numImportedFunctions; i++) {
    ModuleInfo::ImpFuncDef const impFuncDef{moduleInfo_.getImpFuncDef(i)};
    if ((impFuncDef.builtinFunction != BuiltinFunction::UNDEFINED) || (!impFuncDef.linked)) {
      continue;
    }

    NativeSymbol const &nativeSymbol{moduleInfo_.importSymbols[impFuncDef.symbolIndex]};
    if (nativeSymbol.linkage == NativeSymbol::Linkage::DYNAMIC) {
      numDynamicImports++;
      compiler_.output_.write<uint32_t>(impFuncDef.linkDataOffset); // OPBVIF0

      uint32_t const signatureLength{
          static_cast<uint32_t>(strlen_s(nativeSymbol.signature, static_cast<size_t>(ImplementationLimits::maxStringLength)))};
      uint32_t const signatureStepWidth{roundUpToPow2(signatureLength, 2U)};
      compiler_.output_.step(signatureStepWidth); // OPBVIF2 (Padding)
      static_cast<void>(std::memcpy(pSubI(compiler_.output_.ptr(), signatureStepWidth), nativeSymbol.signature,
                                    static_cast<size_t>(signatureLength))); // OPBVIF1
      compiler_.output_.write<uint32_t>(signatureLength);                   // OPBVIF3

      uint32_t const importNameLength{
          static_cast<uint32_t>(strlen_s(nativeSymbol.symbol, static_cast<size_t>(ImplementationLimits::maxStringLength)))};
      uint32_t const importStepWidth{roundUpToPow2(importNameLength, 2U)};
      compiler_.output_.step(importStepWidth); // OPBVIF5 (Padding)
      static_cast<void>(std::memcpy(pSubI(compiler_.output_.ptr(), importStepWidth), nativeSymbol.symbol,
                                    static_cast<size_t>(importNameLength))); // OPBVIF4
      compiler_.output_.write<uint32_t>(importNameLength);                   // OPBVIF6

      uint32_t const moduleNameLength{
          static_cast<uint32_t>(strlen_s(nativeSymbol.moduleName, static_cast<size_t>(ImplementationLimits::maxStringLength)))};
      uint32_t const moduleStepWidth{roundUpToPow2(moduleNameLength, 2U)};
      compiler_.output_.step(moduleStepWidth); // OPBVIF7 (Padding)
      static_cast<void>(std::memcpy(pSubI(compiler_.output_.ptr(), moduleStepWidth), nativeSymbol.moduleName,
                                    static_cast<size_t>(moduleNameLength))); // OPBVIF8
      compiler_.output_.write<uint32_t>(moduleNameLength);                   // OPBVIF9
    }
  }
  compiler_.output_.write<uint32_t>(numDynamicImports);                    // OPBVIF10
  uint32_t const sectionSize{compiler_.output_.size() - sectionStartSize}; //
  compiler_.output_.write<uint32_t>(sectionSize);                          // OPBVIF11
}

// Serialize memory binary section indicating whether the module has a memory
// and what it's initial size is. Max size will be handled inside of the
// module.
void Frontend::serializeMemoryBinarySection() {
  compiler_.output_.write<uint32_t>(moduleInfo_.hasMemory ? moduleInfo_.memoryInitialSize : 0xFFFFFFFF_U32); // OPBVMEM0
}

// Serialize exported global binary section, containing info about exported
// global variables, their types, names, mutability and where they can be found
void Frontend::serializeExportedGlobalsBinarySection() {
  uint32_t const sectionStartSize{compiler_.output_.size()};
  uint32_t numberOfExportedGlobals{0U};
  uint8_t const *stepPtr{moduleInfo_.exports()};
  for (uint32_t i{0U}; i < moduleInfo_.numExports; i++) {
    static_cast<void>(i);
    uint32_t const exportNameLength{readFromPtr<uint32_t>(stepPtr)};
    stepPtr = pAddI(stepPtr, sizeof(uint32_t));
    char const *const exportName{pCast<char const *>(stepPtr)};
    stepPtr = pAddI(stepPtr, exportNameLength);

    WasmImportExportType const exportType{readFromPtr<WasmImportExportType>(stepPtr)};
    stepPtr = pAddI(stepPtr, sizeof(WasmImportExportType));

    uint32_t const index{readFromPtr<uint32_t>(stepPtr)};
    stepPtr = pAddI(stepPtr, sizeof(uint32_t));

    // If this one is not a global, skip export
    if (exportType != WasmImportExportType::GLOBAL) {
      continue;
    }

    ModuleInfo::GlobalDef const &globalDef{moduleInfo_.globals[index]};
    if (globalDef.isMutable) {
      compiler_.output_.write<uint32_t>(globalDef.linkDataOffset); // OPBVMEM0A
    } else {
      // OPBVMEM0B
      switch (globalDef.type) {
      case MachineType::I32:
        compiler_.output_.write<uint32_t>(globalDef.initialValue.u32);
        break;
      case MachineType::I64:
        compiler_.output_.write<uint64_t>(globalDef.initialValue.u64);
        break;
      case MachineType::F32:
        compiler_.output_.write<float>(globalDef.initialValue.f32);
        break;
      case MachineType::F64:
        compiler_.output_.write<double>(globalDef.initialValue.f64);
        break;
      case MachineType::INVALID:
      // GCOVR_EXCL_START
      default: {
        assert(false);
        break;
      }
        // GCOVR_EXCL_STOP
      }
    }
    compiler_.output_.write<uint8_t>(static_cast<uint8_t>(globalDef.isMutable));                                // OPBVEG0
    compiler_.output_.write<SignatureType>(WasmTypeUtil::toSignatureType(MachineTypeUtil::to(globalDef.type))); // OPBVEG1
    compiler_.output_.step(2U);                                                                                 // OPBVEG2 (Padding)

    uint32_t const stepWidth{roundUpToPow2(exportNameLength, 2U)};
    compiler_.output_.step(stepWidth); // OPBVEG4 (Padding)
    static_cast<void>(std::memcpy(pSubI(compiler_.output_.ptr(), stepWidth), exportName,
                                  static_cast<size_t>(exportNameLength))); // OPBVEG3
    compiler_.output_.write<uint32_t>(exportNameLength);                   // OPBVEG5

    numberOfExportedGlobals++;
  }

  compiler_.output_.write<uint32_t>(numberOfExportedGlobals);              // OPBVEG6
  uint32_t const sectionSize{compiler_.output_.size() - sectionStartSize}; //
  compiler_.output_.write<uint32_t>(sectionSize);                          // OPBVEG7
}

// Serialize exported function binary section, containing info about exported
// functions, their signatures, names and how they can be called
void Frontend::serializeExportedFunctionBinarySection() {
  uint32_t const sectionStartSize{compiler_.output_.size()};
  uint32_t numberOfExportedFunctions{0U};

  // coverity[autosar_cpp14_a8_5_2_violation]
  auto const produceWrapper = [this, &numberOfExportedFunctions](char const *const exportName, uint32_t const exportNameLength,
                                                                 uint32_t const fncIndex, bool const isDirectExport) {
    // coverity[autosar_cpp14_a5_1_8_violation]
    writePaddedBinaryBlob(FunctionRef<void()>([this, fncIndex]() {
      compiler_.backend_.emitFunctionEntryPoint(fncIndex);
    })); // OPBVF0

    uint32_t const sigIndex{moduleInfo_.getFncSigIndex(fncIndex)};
    if (moduleInfo_.functionIsBuiltin(fncIndex)) {
      throw ImplementationLimitationException(ErrorCode::Cannot_export_builtin_function);
    }

    uint32_t const signatureLength{moduleInfo_.typeOffsets[sigIndex + 1U] - moduleInfo_.typeOffsets[sigIndex]};
    char const *const signature{pAddI(pCast<char const *>(moduleInfo_.types()), moduleInfo_.typeOffsets[sigIndex])};

    uint32_t const signatureStepLength{roundUpToPow2(signatureLength, 2U)};
    compiler_.output_.step(signatureStepLength); // OPBVEF4 (Padding)
    // OPBVEF3
    // NOLINTNEXTLINE(clang-analyzer-core.NonNullParamChecker,clang-analyzer-unix.cstring.NullArg)
    static_cast<void>(std::memcpy(pSubI(compiler_.output_.ptr(), signatureStepLength), signature, static_cast<size_t>(signatureLength)));
    compiler_.output_.write<uint32_t>(signatureLength); // OPBVEF5
    uint32_t const functionEntryPointOffset{compiler_.output_.size()};

    if (exportNameLength > 0U) {
      uint32_t const nameStepLength{roundUpToPow2(exportNameLength, 2U)};
      compiler_.output_.step(nameStepLength); // OPBVEF7 (Padding)
      static_cast<void>(std::memcpy(pSubI(compiler_.output_.ptr(), nameStepLength), exportName,
                                    static_cast<size_t>(exportNameLength))); // OPBVEF6
    }
    compiler_.output_.write<uint32_t>(exportNameLength);                           // OPBVEF8
    compiler_.output_.write<uint32_t>(isDirectExport ? fncIndex : 0xFF'FF'FF'FFU); // OPBVEF9

    if (moduleInfo_.hasTable && moduleInfo_.tableIsExported) {
      // double loop, maybe optimize that in some
      for (uint32_t j{0U}; j < moduleInfo_.tableInitialSize; j++) {
        if (moduleInfo_.tableElements[j].fncIndex == fncIndex) {
          assert(moduleInfo_.tableElements[j].fncIndex != 0xFF'FF'FF'FF && "Function index out of range");
          moduleInfo_.tableElements[j].exportWrapperOffset = functionEntryPointOffset;
        }
      }
    }
    numberOfExportedFunctions++;
  };

  uint8_t const *stepPtr{moduleInfo_.exports()};
  for (uint32_t i{0U}; i < moduleInfo_.numExports; i++) {
    static_cast<void>(i);
    uint32_t const exportNameLength{readFromPtr<uint32_t>(stepPtr)};
    stepPtr = pAddI(stepPtr, sizeof(uint32_t));
    char const *const exportName{pCast<char const *>(stepPtr)};
    stepPtr = pAddI(stepPtr, exportNameLength);

    WasmImportExportType const exportType{readFromPtr<WasmImportExportType>(stepPtr)};
    stepPtr = pAddI(stepPtr, sizeof(WasmImportExportType));

    uint32_t const index{readFromPtr<uint32_t>(stepPtr)};
    stepPtr = pAddI(stepPtr, sizeof(uint32_t));

    // If this one is not a function, skip export
    if (exportType != WasmImportExportType::FUNC) {
      continue;
    }

    produceWrapper(exportName, exportNameLength, index, true);
  }

  if (moduleInfo_.hasTable && moduleInfo_.tableIsExported) {
    for (uint32_t i{0U}; i < moduleInfo_.tableInitialSize; i++) {
      // Check if already exported
      if (moduleInfo_.tableElements[i].exportWrapperOffset != 0xFF'FF'FF'FFU) {
        continue;
      }
      if (moduleInfo_.tableElements[i].fncIndex == 0xFF'FF'FF'FFU) {
        continue;
      }
      produceWrapper("", 0U, moduleInfo_.tableElements[i].fncIndex, false);
    }
  }

  compiler_.output_.write<uint32_t>(numberOfExportedFunctions);            // OPBVEF12
  uint32_t const sectionSize{compiler_.output_.size() - sectionStartSize}; //
  compiler_.output_.write<uint32_t>(sectionSize);                          // OPBVEF13
}

void Frontend::serializeLinkStatusSection() {
  for (uint32_t i{0U}; i < moduleInfo_.numImportedFunctions; i++) {
    ModuleInfo::ImpFuncDef const impFuncDef{moduleInfo_.getImpFuncDef(i)};
    uint8_t const linkStatus{impFuncDef.linked ? 1_U8 : 0_U8};
    compiler_.output_.write<uint8_t>(linkStatus);
  }
  uint32_t const paddingLength{deltaToNextPow2(moduleInfo_.numImportedFunctions, 2U)};
  compiler_.output_.step(paddingLength);                               // OPBILS2 (Padding)
  compiler_.output_.write<uint32_t>(moduleInfo_.numImportedFunctions); // OPBILS3
}

// Serialize table binary section, containing the table entries ("function
// pointers") and their signatures. The pointers contain the offset to the
// called function body from the start of OPBVT1
void Frontend::serializeTableBinarySection() {
  constexpr uint32_t unknownValue{0xFF'FF'FF'FFU};
  for (uint32_t i{0U}; i < moduleInfo_.tableInitialSize; i++) {
    uint32_t const elementFunctionIndex{moduleInfo_.tableElements[i].fncIndex};

    if (elementFunctionIndex == unknownValue) {
      // Write function binary offset and signature index
      compiler_.output_.write<uint32_t>(unknownValue); // OPBVT0
      compiler_.output_.write<uint32_t>(unknownValue); // OPBVT1
    } else {
      uint32_t const sigIndex{moduleInfo_.getFncSigIndex(elementFunctionIndex)};
      bool const linked{moduleInfo_.functionIsLinked(elementFunctionIndex)};

      // Calculate the offset from here and emit it
      if (linked) {
        uint32_t const functionBinaryOffset{moduleInfo_.wasmFncBodyBinaryPositions[elementFunctionIndex]};
        assert(functionBinaryOffset != unknownValue && "Function body not found at serialization");
        compiler_.output_.write<uint32_t>(functionBinaryOffset); // OPBVT0
      } else {
        compiler_.output_.write<uint32_t>(0_U32); // OPBVT0
      }
      compiler_.output_.write<uint32_t>(sigIndex); // OPBVT1
    }
  }
  compiler_.output_.write<uint32_t>(moduleInfo_.tableInitialSize); // OPBVT2
}

void Frontend::serializeTableEntryFunctionWrapperSection() {
  for (uint32_t i{0U}; i < moduleInfo_.tableInitialSize; i++) {
    uint32_t const functionEnctryOffset{moduleInfo_.tableElements[i].exportWrapperOffset};
    compiler_.output_.write<uint32_t>(functionEnctryOffset); // OBBTE1
  }
  compiler_.output_.write<uint32_t>(moduleInfo_.tableInitialSize); // OBBTE0
}

// Serialize general metadata about the module, like how much link data
// space needs to be reserved, the version of the VB compiler this was
// produced with and the module binary length in bytes (excluding the size
// value)
void Frontend::serializeModuleMetadataBinarySection() {
  compiler_.output_.write<uint32_t>(moduleInfo_.linkDataLength); // OPBVMET0
#if LINEAR_MEMORY_BOUNDS_CHECKS
  compiler_.output_.write<uint32_t>(0xFFFF'FFFF_U32); // OPBVMET1
#else
  compiler_.output_.write<uint32_t>(compiler_.output_.size() - moduleInfo_.helperFunctionBinaryPositions.landingPad); // OPBVMET1
#endif
  uint32_t const stacktraceEntry{(compiler_.getStacktraceRecordCount() & 0x7F'FF'FF'FFU) | (compiler_.getDebugMode() ? 0x80'00'00'00U : 0U)};
  compiler_.output_.write<uint32_t>(stacktraceEntry);             // OPBVMET2
  compiler_.output_.write<uint32_t>(BinaryModule::versionNumber); // OPBVER
  compiler_.output_.write<uint32_t>(compiler_.output_.size());    // OPBVMET3
}

void Frontend::parseCustomSection(uint8_t const *const sectionEnd, FunctionRef<void()> const &preNameSectionAction) {
  uint32_t const sectionNameLength{br_.readLEB128<uint32_t>()};
  char const *const sectionName{pCast<char const *>(br_.getPtr())};

  // Validate the section name
  br_.step(sectionNameLength);
  validateUTF8(sectionName, static_cast<size_t>(sectionNameLength));

  constexpr std::array<char, 5> nameSectionName{"name"};
  if ((sectionNameLength == (static_cast<uint32_t>(nameSectionName.size()) - 1U)) &&
      (std::strncmp(nameSectionName.data(), sectionName, static_cast<size_t>(sectionNameLength)) == 0)) {
    if (preNameSectionAction.notNull()) {
      preNameSectionAction();
    }

    uint32_t const sectionStartSize{compiler_.output_.size()};
    uint32_t numFunctionNames{0_U32};

    if (compiler_.getStacktraceRecordCount() > 0U) {
      while (br_.getPtr() < sectionEnd) {
        enum class NameSubSectionType : uint8_t { MODULE, FUNCTION, LOCAL };

        NameSubSectionType const subsectionType{br_.readByte<NameSubSectionType>()};
        uint32_t const subsectionSize{br_.readLEB128<uint32_t>()};

        uint8_t const *const subSectionPosAfterSize{br_.getPtr()};
        static_cast<void>(subSectionPosAfterSize);
        // We only do weak validation of this section. If a functionIndex is not an actual function, we still see it as
        // valid For size mismatch we still throw a ValidationException
        if (subsectionType == NameSubSectionType::FUNCTION) {
          numFunctionNames = br_.readLEB128<uint32_t>();

          for (uint32_t i{0U}; i < numFunctionNames; i++) {
            uint32_t const functionIndex{br_.readLEB128<uint32_t>()};
            static_cast<void>(functionIndex);

            uint32_t const nameLength{br_.readLEB128<uint32_t>()};
            char const *const name{pCast<char const *>(br_.getPtr())};

            br_.step(nameLength);
            validateUTF8(name, static_cast<size_t>(nameLength));

            uint32_t const nameStepLength{roundUpToPow2(nameLength, 2U)};
            compiler_.output_.step(nameStepLength); // OPBFN1 (Padding)
            static_cast<void>(std::memcpy(pSubI(compiler_.output_.ptr(), nameStepLength), name,
                                          static_cast<size_t>(nameLength))); // OPBFN0
            compiler_.output_.write<uint32_t>(nameLength);                   // OPBFN2
            compiler_.output_.write<uint32_t>(functionIndex);                // OPBFN3
          }

          uint8_t const *const subSectionEnd{pAddI(subSectionPosAfterSize, subsectionSize)};
          if (br_.getPtr() != subSectionEnd) {
            if (compiler_.logging() != nullptr) {
              *compiler_.logging() << "Name section function subsection size mismatch" << &vb::endStatement<vb::LogLevel::LOGERROR>;
            }
            throw ValidationException(ErrorCode::Subsection_size_mismatch);
          }

          // This subsection was properly validated
        } else {
          br_.step(subsectionSize);
        }
      }
    }

    compiler_.output_.write<uint32_t>(numFunctionNames);                     // OPBFN4
    uint32_t const sectionSize{compiler_.output_.size() - sectionStartSize}; //
    compiler_.output_.write<uint32_t>(sectionSize);                          // OPBFN5

    br_.jumpTo(sectionEnd);
    return;
  }

  // Skip the rest
  br_.jumpTo(sectionEnd);
}

// This action is performed directly after a specific section or where it would occur if it isn't present in the module
void Frontend::postSectionAction(SectionType const sectionType) {
  switch (sectionType) {
  case SectionType::TYPE: {
    // Set pointer to compiler memory where function definitions can be stored. This must be available for both the
    // import and function section, even if one or both of the two sections is missing
    moduleInfo_.fncDefs.setOffset(memory_.size(), memory_);
    break;
  }
  case SectionType::FUNCTION: {
    // Initialize an array in memory containing the binary offsets (u32) in bytes (from the start of the binary) to
    // functions using the Wasm calling convention These functions can be either in-module Wasm functions or wrapper
    // functions for calling imported functions via a table
    moduleInfo_.wasmFncBodyBinaryPositions.setOffset(memory_.alignForType<uint32_t>(), memory_);

    uint32_t const arrayLength{moduleInfo_.numTotalFunctions * static_cast<uint32_t>(sizeof(uint32_t))};
    memory_.step(arrayLength); // Reserve space for all functions

    // Initialize the whole array to 0xFF because 0xFFFF'FFFF means it is  uninitialized
    static_cast<void>(std::memset(moduleInfo_.wasmFncBodyBinaryPositions(), 0xFF, static_cast<size_t>(arrayLength)));
    break;
  }
  case SectionType::CODE: {
    if (moduleInfo_.numFunctionBodiesProduced < (moduleInfo_.numTotalFunctions - moduleInfo_.numImportedFunctions)) {
      throw ValidationException(ErrorCode::Missing_function_bodies);
    }
    break;
  }
  case SectionType::DATA: {
    if (moduleInfo_.numDataSegments == UINT32_MAX) {
      moduleInfo_.numDataSegments = 0U;
    }
    // Write the number of data segments to the output binary, this needs to be always present, irrespective of whether
    // data section is there or not
    compiler_.output_.write<uint32_t>(moduleInfo_.numDataSegments); // OPBVLM4
    break;
  }
  default:
    break;
  }
}

void Frontend::startCompilation(bool const forceHighRegisterPressureForTesting) {
  // Reset compiler memory if another module has previously been compiled with
  // the same compiler instance
  memory_.flush();

  // Reset moduleInfo for same reason
  moduleInfo_ = ModuleInfo();
  moduleInfo_.forceHighRegisterPressureForTesting = forceHighRegisterPressureForTesting;
  moduleInfo_.stacktraceRecordCount = compiler_.getStacktraceRecordCount();
  moduleInfo_.debugMode = compiler_.getDebugMode();

  moduleInfo_.importSymbols = symbolList_.data();

  memory_.reserve(0xFFU);

  if (br_.getPtr() == nullptr) {
    throw ValidationException(ErrorCode::Empty_input);
  }
  validateMagicNumber();
  validateVersion();

  bool nameSectionHandled{false};
  size_t expectedSectionSequenceIndex{0U};

  writePaddedBinaryBlob(FunctionRef<void()>([this]() {
    common_.emitGenericTrapHandler();
  })); // OPBVF0

  // coverity[autosar_cpp14_a8_5_2_violation]
  auto const moveToTargetSection = [this, &expectedSectionSequenceIndex](SectionType const sectionType) -> void {
    // coverity[autosar_cpp14_a8_5_2_violation]
    constexpr auto sectionOrder =
        make_array<SectionType>(SectionType::TYPE, SectionType::IMPORT, SectionType::FUNCTION, SectionType::TABLE, SectionType::MEMORY,
                                SectionType::GLOBAL, SectionType::EXPORT, SectionType::START, SectionType::ELEMENT, SectionType::DATA_COUNT,
                                SectionType::CODE, SectionType::DATA, SectionType::PLACEHOLDER);

    // Execute post section actions for all skipped sections
    for (; expectedSectionSequenceIndex < sectionOrder.size(); expectedSectionSequenceIndex++) {
      SectionType const currentExpectedSectionType{sectionOrder[expectedSectionSequenceIndex]};
      if (currentExpectedSectionType == sectionType) {
        break;
      }
      postSectionAction(currentExpectedSectionType);
    }
    if (expectedSectionSequenceIndex == sectionOrder.size()) {
      // If we cannot found current section type between expectedSectionSequenceIndex and last, the order of the
      // sections is wrong or the sections are repeated.
      throw ValidationException(ErrorCode::Duplicate_section_or_sections_in_wrong_order);
    }
  };

  while (br_.hasNextByte()) {
    SectionType const sectionType{br_.readByte<SectionType>()};
    uint32_t const sectionSize{br_.readLEB128<uint32_t>()};
    if (sectionSize == 0U) {
      throw ValidationException(ErrorCode::Section_of_size_0);
    }
    if (sectionSize > br_.getBytesLeft()) {
      throw ValidationException(ErrorCode::Section_size_extends_past_module_size);
    }

    uint8_t const *const sectionPosAfterSize{br_.getPtr()};
    uint8_t const *const sectionEnd{pAddI(sectionPosAfterSize, sectionSize)};

    // Custom sections are allowed anywhere, can be interleaved
    if (sectionType != SectionType::CUSTOM) {
      // coverity[autosar_cpp14_a4_5_1_violation]
      moveToTargetSection(sectionType);
    }

    switch (sectionType) {
    case SectionType::CUSTOM: {
      // coverity[autosar_cpp14_a5_1_4_violation]
      parseCustomSection(sectionEnd, FunctionRef<void()>([&nameSectionHandled, &moveToTargetSection]() {
                           if (nameSectionHandled) {
                             throw ValidationException(ErrorCode::Multiple_name_sections_encountered);
                           }
                           nameSectionHandled = true;
                           // coverity[autosar_cpp14_a4_5_1_violation]
                           moveToTargetSection(SectionType::PLACEHOLDER);
                         }));
      break;
    }
    case SectionType::TYPE:
      parseTypeSection();
      break;
    case SectionType::IMPORT:
      parseImportSection();
      break;
    case SectionType::FUNCTION:
      parseFunctionSection();
      break;
    case SectionType::TABLE:
      parseTableSection();
      break;
    case SectionType::MEMORY:
      parseMemorySection();
      break;
    case SectionType::GLOBAL:
      parseGlobalSection();
      break;
    case SectionType::EXPORT:
      parseExportSection();
      break;
    case SectionType::START:
      parseStartSection();
      break;
    case SectionType::ELEMENT:
      parseElementSection();
      break;
    case SectionType::CODE:
      parseCodeSection();
      break;
    case SectionType::DATA:
      parseDataSection();
      break;
    case SectionType::DATA_COUNT:
      parseDataCountSection();
      break;
    default:
      throw ValidationException(ErrorCode::Invalid_section_type);
    }

    if (sectionType != SectionType::CUSTOM) {
      postSectionAction(sectionType);
      expectedSectionSequenceIndex++;
    }

    if (br_.getPtr() != sectionEnd) {
      if (compiler_.logging() != nullptr) {
        *compiler_.logging() << "Section " << static_cast<uint32_t>(sectionType) << " size mismatch" << &vb::endStatement<vb::LogLevel::LOGERROR>;
      }
      throw ValidationException(ErrorCode::Section_size_mismatch);
    }

#if ENABLE_EXTENSIONS
    if (compiler_.getAnalytics() != nullptr) {
      compiler_.getAnalytics()->notifySectionParsingDone(sectionType, memory_.size());
    }
#endif
  }
  // coverity[autosar_cpp14_a4_5_1_violation]
  moveToTargetSection(SectionType::PLACEHOLDER);

  if (!nameSectionHandled) {
    nameSectionHandled = true;
    compiler_.output_.write<uint32_t>(0_U32); // OPBFN4
    compiler_.output_.write<uint32_t>(4_U32); // OPBFN5
  }

  serializeStartFunctionSection();
  serializeWasmGlobalsBinarySection();
  serializeDynamicFunctionImportBinarySection();
  serializeMemoryBinarySection();
  serializeExportedGlobalsBinarySection();
  serializeExportedFunctionBinarySection();
  serializeLinkStatusSection();
  serializeTableBinarySection();
  serializeTableEntryFunctionWrapperSection();
  serializeModuleMetadataBinarySection();

#if ENABLE_EXTENSIONS
  if (compiler_.getAnalytics() != nullptr) {
    compiler_.getAnalytics()->notifySerializationDone(memory_.size());
  }
#endif
}

void Frontend::popBlockAndPushReturnValues(Stack::iterator const blockIt) VB_NOEXCEPT {
  if (blockIt != stack_.last()) {
    Stack::SubChain const returnValues{stack_.split(blockIt)};
    // GCOVR_EXCL_START
    assert(blockIt == stack_.last());
    // GCOVR_EXCL_STOP
    stack_.pop();

    if (!stack_.empty()) {
      returnValues.begin()->sibling = stack_.last();
    } else {
      returnValues.begin()->sibling = Stack::iterator();
    }

    stack_.contactAtEnd(returnValues);
  } else {
    stack_.pop();
  }
}
} // namespace vb

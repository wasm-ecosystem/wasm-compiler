///
/// @file Runtime.cpp
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
#include <cstdlib>
#include <cstring>
#include <functional>
#include <utility>

#include "Runtime.hpp"
#include "TrapException.hpp"

#include "src/config.hpp"
#include "src/core/common/BinaryModule.hpp"
#include "src/core/common/ExtendableMemory.hpp"
#include "src/core/common/FunctionRef.hpp"
#include "src/core/common/ILogger.hpp"
#include "src/core/common/NativeSymbol.hpp"
#include "src/core/common/SignatureType.hpp"
#include "src/core/common/Span.hpp"
#include "src/core/common/TrapCode.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/WasmConstants.hpp"
#include "src/core/common/basedataoffsets.hpp"
#include "src/core/common/implementationlimits.hpp"
#include "src/core/common/util.hpp"
#include "src/core/compiler/common/MachineType.hpp"
#include "src/core/runtime/MemoryHelper.hpp"

#if defined(JIT_TARGET_TRICORE)
#if TC_LINK_AUX_FNCS_DYNAMICALLY
#include "src/core/compiler/backend/tricore/tricore_aux.hpp"
#endif
#endif

namespace vb {

static_assert(sizeof(WasmValue) == 8U, "WasmValue size mismatch");

#if LINEAR_MEMORY_BOUNDS_CHECKS
Runtime::Runtime(Runtime &&other) VB_NOEXCEPT : disabled_(other.disabled_),
                                                queuedStartFncOffset_(other.queuedStartFncOffset_),
                                                jobMemory_(std::move(other.jobMemory_)),
                                                // coverity[autosar_cpp14_a12_8_4_violation]
                                                // coverity[autosar_cpp14_a8_4_5_violation]
                                                binaryModule_(other.binaryModule_) {
  other.disabled_ = true;
}
#else
Runtime::Runtime(Runtime &&other) VB_NOEXCEPT : disabled_(other.disabled_),
                                                queuedStartFncOffset_(other.queuedStartFncOffset_),
                                                jobMemoryStart_(other.jobMemoryStart_),
                                                memoryExtendFunction_(std::move(other.memoryExtendFunction_)),
                                                memoryProbeFunction_(std::move(other.memoryProbeFunction_)),
                                                memoryShrinkFunction_(std::move(other.memoryShrinkFunction_)),
                                                memoryUsageFunction_(std::move(other.memoryUsageFunction_)),
                                                // coverity[autosar_cpp14_a8_4_5_violation]
                                                // coverity[autosar_cpp14_a12_8_4_violation]
                                                binaryModule_(other.binaryModule_) {
  other.disabled_ = true;
  other.jobMemoryStart_ = nullptr;
}
#endif

Runtime &Runtime::operator=(Runtime &&other) & VB_NOEXCEPT {
  swap(*this, std::move(other));
  return *this;
}

void Runtime::checkIsReady(bool const mustHaveStarted) const {
  if (disabled_) {
    throw RuntimeError(ErrorCode::Runtime_is_disabled);
  }
  if (mustHaveStarted && (queuedStartFncOffset_ != 0xFF'FF'FF'FEU)) {
    throw RuntimeError(ErrorCode::Module_not_initialized__Call_start_function_first_);
  }
}

void Runtime::init(Span<NativeSymbol const> const &dynamicallyLinkedSymbols, void *const ctx) {
  if ((pToNum(getMemoryBase()) % 8U) != 0U) {
    throw RuntimeError(ErrorCode::Base_of_job_memory_not_8_byte_aligned);
  }

  updateBinaryModule(binaryModule_);
  queuedStartFncOffset_ = initializeModule(dynamicallyLinkedSymbols, ctx);
}

void Runtime::start() {
  if (queuedStartFncOffset_ == 0xFF'FF'FF'FEU) {
    throw RuntimeError(ErrorCode::Start_function_has_already_been_called);
  }
  // Call start function
  if (queuedStartFncOffset_ != 0xFF'FF'FF'FFU) {
    RawModuleFunction(*this, queuedStartFncOffset_)(nullptr, nullptr);
  }
  queuedStartFncOffset_ = 0xFF'FF'FF'FEU;
}

// Deserialize the binary module and initialize it. This means reserving space
// for the job memory within the given ExtendableMemory, copying initial
// values of globals to the link data and linking dynamically linked
// functions by copying their pointers to the corresponding location within the
// link data
uint32_t Runtime::initializeModule(Span<NativeSymbol const> const &dynamicallyLinkedSymbols, void *const ctx) {
  uint32_t const linkDataLength{binaryModule_.getLinkDataLength()};

  uint32_t const basedataLength{Basedata::length(linkDataLength, binaryModule_.getStacktraceEntryCount())};

  // Check if length of memory region is enough to fit base data (link data etc.) in there, without linear memory
  // contents
#if LINEAR_MEMORY_BOUNDS_CHECKS
  jobMemory_.resize(basedataLength);
#endif

  writeToPtr<uintptr_t>(pAddI(getMemoryBase(), basedataLength - static_cast<uint32_t>(Basedata::FromEnd::binaryModuleStartAddressOffset)),
                        pToNum(binaryModule_.getStartAddress()));

  writeToPtr<uintptr_t>(pAddI(getMemoryBase(), basedataLength - static_cast<uint32_t>(Basedata::FromEnd::tableAddressOffset)),
                        pToNum(binaryModule_.getTableStart()));

  writeToPtr<uintptr_t>(pAddI(getMemoryBase(), basedataLength - static_cast<uint32_t>(Basedata::FromEnd::linkStatusAddressOffset)),
                        pToNum(binaryModule_.getLinkStatusStart()));

  // Write initial memory size to metadata
  assert(basedataLength >= static_cast<uint32_t>(Basedata::FromEnd::linMemWasmSize) && "basedataLength must not be less than linMemWasmSize");

  writeToPtr<uint32_t>(pAddI(getMemoryBase(), basedataLength - static_cast<uint32_t>(Basedata::FromEnd::linMemWasmSize)),
                       binaryModule_.getInitialMemorySize());

  writeToPtr<void const *>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::customCtxOffset), ctx);

  setMemoryHelperPtr();

  uint8_t const *dynamicallyImportedFunctionsSectionCursor{binaryModule_.getDynamicallyImportedFunctionsSectionEnd()};
  uint32_t const numDynamicallyImportedFunctions{readNextValue<uint32_t>(&dynamicallyImportedFunctionsSectionCursor)}; // OPBVIF10
  for (uint32_t i{0U}; i < numDynamicallyImportedFunctions; i++) {
    uint32_t const moduleNameLength{readNextValue<uint32_t>(&dynamicallyImportedFunctionsSectionCursor)};                              // OPBVIF9
    dynamicallyImportedFunctionsSectionCursor = pSubI(dynamicallyImportedFunctionsSectionCursor, roundUpToPow2(moduleNameLength, 2U)); // OPBVIF8
    char const *const moduleName{pCast<char const *>(dynamicallyImportedFunctionsSectionCursor)};                                      // OPBVIF7

    uint32_t const importNameLength{readNextValue<uint32_t>(&dynamicallyImportedFunctionsSectionCursor)};                              // OPBVIF6
    dynamicallyImportedFunctionsSectionCursor = pSubI(dynamicallyImportedFunctionsSectionCursor, roundUpToPow2(importNameLength, 2U)); // OPBVIF5
    char const *const importName{pCast<char const *>(dynamicallyImportedFunctionsSectionCursor)};                                      // OPBVIF4

    uint32_t const signatureLength{readNextValue<uint32_t>(&dynamicallyImportedFunctionsSectionCursor)};                              // OPBVIF3
    dynamicallyImportedFunctionsSectionCursor = pSubI(dynamicallyImportedFunctionsSectionCursor, roundUpToPow2(signatureLength, 2U)); // OPBVIF2
    char const *const signature{pCast<char const *>(dynamicallyImportedFunctionsSectionCursor)};                                      // OPBVIF1
    uint32_t const linkDataOffset{readNextValue<uint32_t>(&dynamicallyImportedFunctionsSectionCursor)};                               // OPBVIF0

    bool found{false};
    for (size_t symbolIndex{0U}; symbolIndex < dynamicallyLinkedSymbols.size(); symbolIndex++) {
      NativeSymbol const symbol{dynamicallyLinkedSymbols[symbolIndex]};
      bool const moduleNameMatches{
          (static_cast<uint32_t>(strlen_s(symbol.moduleName, static_cast<size_t>(ImplementationLimits::maxStringLength))) == moduleNameLength) &&
          (std::strncmp(moduleName, symbol.moduleName, static_cast<size_t>(moduleNameLength)) == 0)};
      if (moduleNameMatches) {
        bool const symbolNameMatches{
            (static_cast<uint32_t>(strlen_s(symbol.symbol, static_cast<size_t>(ImplementationLimits::maxStringLength))) == importNameLength) &&
            (std::strncmp(importName, symbol.symbol, static_cast<size_t>(importNameLength)) == 0)};
        if (symbolNameMatches) {
          bool const signatureMatches{
              (static_cast<uint32_t>(strlen_s(symbol.signature, static_cast<size_t>(ImplementationLimits::maxStringLength))) == signatureLength) &&
              (std::strncmp(signature, symbol.signature, static_cast<size_t>(signatureLength)) == 0)};
          if (signatureMatches) {
            assert(linkDataOffset + sizeof(symbol.ptr) <= linkDataLength && "Bookkeeping data overflow");
            static_assert(sizeof(symbol.ptr) <= 8U, "Pointer datatype too big");
            writeToPtr<void const *>(pAddI(getMemoryBase(), Basedata::FromStart::linkData + linkDataOffset), symbol.ptr);
            found = true;
            break;
          }
        }
      }
    }
    if (!found) {
      throw LinkingException(ErrorCode::Dynamic_import_not_resolved);
    }
  }

  uint8_t const *mutableGlobalCursor{binaryModule_.getMutableGlobalsSectionEnd()};

  uint32_t const numMutableGlobals{readNextValue<uint32_t>(&mutableGlobalCursor)}; // OPBVNG4
  for (uint32_t i{0U}; i < numMutableGlobals; i++) {
    mutableGlobalCursor = pSubI(mutableGlobalCursor, 3U);                                                // Padding (OPBVNG3)
    MachineType const type{readNextValue<MachineType>(&mutableGlobalCursor)};                            // OPBVNG2
    uint16_t const linkDataOffset{static_cast<uint16_t>(readNextValue<uint32_t>(&mutableGlobalCursor))}; // OPBVNG1

    uint32_t const variableSize{MachineTypeUtil::getSize(type)};
    mutableGlobalCursor = pSubI(mutableGlobalCursor, variableSize);
    assert(static_cast<uint32_t>(linkDataOffset + variableSize) <= linkDataLength && "Bookkeeping data overflow");
    static_cast<void>(std::memcpy(pAddI(getMemoryBase(), static_cast<size_t>(Basedata::FromStart::linkData) + static_cast<size_t>(linkDataOffset)),
                                  mutableGlobalCursor,
                                  static_cast<size_t>(variableSize))); // OPBVNG0
  }

  // SECTION: Data
  uint32_t const linearMemoryBaseOffset{basedataLength};
  uint8_t const *dataSegmentsCursor{binaryModule_.getDataSegmentsEnd()};
  // coverity[autosar_cpp14_a7_1_1_violation] NOLINTNEXTLINE(misc-const-correctness)
  uint32_t maximumDataOffset{0U};
  for (uint32_t i{0U}; i < binaryModule_.getNumDataSegments(); i++) {
    static_cast<void>(i);
    uint32_t const dataSegmentStart{readNextValue<uint32_t>(&dataSegmentsCursor)};      // OPBVLM3
    uint32_t const dataSegmentSize{readNextValue<uint32_t>(&dataSegmentsCursor)};       // OPBVLM2
    dataSegmentsCursor = pSubI(dataSegmentsCursor, roundUpToPow2(dataSegmentSize, 2U)); // OPBVLM1

#if LINEAR_MEMORY_BOUNDS_CHECKS
    uint32_t const maximumSegmentOffset{dataSegmentStart + dataSegmentSize};
    if (maximumSegmentOffset > maximumDataOffset) {
      // Check if linear memory can accommodate this segment, otherwise request extension
      jobMemory_.resize(linearMemoryBaseOffset + maximumSegmentOffset);
      // Init linear memory data to zeros
      static_cast<void>(std::memset(pAddI(getMemoryBase(), linearMemoryBaseOffset + maximumDataOffset), 0x00,
                                    static_cast<size_t>(maximumSegmentOffset) - static_cast<size_t>(maximumDataOffset)));
      maximumDataOffset = maximumSegmentOffset;
    }
#endif

    if (dataSegmentSize > 0U) {
#if !LINEAR_MEMORY_BOUNDS_CHECKS
      if (!probeLinearMemory((dataSegmentStart + dataSegmentSize) - 1U)) {
        throw RuntimeError(ErrorCode::Could_not_extend_linear_memory);
      }
#endif
      uint8_t const *const data{dataSegmentsCursor}; // OPBVLM0
      static_cast<void>(std::memcpy(pAddI(getMemoryBase(), linearMemoryBaseOffset + dataSegmentStart), data, static_cast<size_t>(dataSegmentSize)));
    }
  }

  uint32_t const actualMemorySize{binaryModule_.hasLinearMemory() ? maximumDataOffset : 0U};
  // Write it to metadata memory, everything has already been initialized to zero
  uint8_t *const actualMemoryBaseData{pAddI(getMemoryBase(), basedataLength - static_cast<uint32_t>(Basedata::FromEnd::actualLinMemByteSize))};

  writeToPtr<uint32_t>(actualMemoryBaseData, actualMemorySize);

  assert(getMemoryBase() + Basedata::FromStart::linkData + linkDataLength ==
             getMemoryBase() + basedataLength - Basedata::FromEnd::getLast(binaryModule_.getStacktraceEntryCount()) &&
         "Metadata size error");

#if BUILTIN_FUNCTIONS
  unlinkMemory();
  clearTraceBuffer();
#endif

#if INTERRUPTION_REQUEST
  resetStatusFlags();
#endif

#if ACTIVE_STACK_OVERFLOW_CHECK
  try {
    constexpr uintptr_t highPtr{UINTPTR_MAX -
                                // coverity[autosar_cpp14_m0_1_2_violation]
                                static_cast<uintptr_t>((64U > STACKSIZE_LEFT_BEFORE_NATIVE_CALL) ? 64U : STACKSIZE_LEFT_BEFORE_NATIVE_CALL)};
    setStackFence(numToP<void *>(highPtr));
  } catch (vb::RuntimeError const &e) {
    static_cast<void>(e);
  } catch (...) {
    // GCOVR_EXCL_START
    UNREACHABLE(return 0U, "Should not have other exception type");
    // GCOVR_EXCL_STOP
  }
#endif

#if defined(JIT_TARGET_TRICORE)
#if TC_LINK_AUX_FNCS_DYNAMICALLY
  writeToPtr<uint32_t>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::arrDynSimpleFncCallsPtr),
                       static_cast<uint32_t>(pToNum(&tc::aux::getSoftfloatImplementationFunctions())));
#endif
#endif

  resetStacktraceAndDebugRecords();
  resetTrapInfo();

  return binaryModule_.getStartFunctionBinaryOffset();
}

void Runtime::tryTrap(TrapCode const trapCode) const VB_NOEXCEPT {
  if (hasActiveFrame()) {
    // The trap func must be no except, because the code after trapWrapperFncPtr is unreachable. If it's not noexcept, it can break C++ unwind stack
    // coverity[autosar_cpp14_a5_2_4_violation]
    // coverity[autosar_cpp14_a8_4_7_violation] NOLINTNEXTLINE(cppcoreguidelines-pro-type-reinterpret-cast)
    void (*const trapWrapperFncPtr)(uint8_t *, uint32_t) VB_NOEXCEPT{reinterpret_cast<void (*)(uint8_t *, uint32_t) VB_NOEXCEPT>(getTrapFnc())};

    // Pass base pointer and the TrapCode
    trapWrapperFncPtr(getLinearMemoryBase(), static_cast<uint32_t>(trapCode));
    // GCOVR_EXCL_START
    UNREACHABLE(return, "Will never return");
    // GCOVR_EXCL_STOP
  }
  // Otherwise do nothing, if this function returns it "failed" because no Wasm function was currently executing
}

uint32_t Runtime::getInitialLinMemSizeInPages() const VB_NOEXCEPT {
  // No memory defined
  if (!binaryModule_.hasLinearMemory()) {
    return 0U;
  } else {
    return binaryModule_.getInitialMemorySize();
  }
}

#if ACTIVE_STACK_OVERFLOW_CHECK
void Runtime::setStackFence(void const *const stackFence) const {
// Store fence plus 64 so we do not have to check for any changes <= 64 bytes
#if (CXX_TARGET != ISA_TRICORE)
  static_assert(sizeof(void *) == 8, "Pointer size mismatch");
#else
  static_assert(sizeof(void *) == 4, "Pointer size mismatch");
#endif
  if (pToNum(stackFence) > (UINTPTR_MAX - 64U)) {
    throw RuntimeError(ErrorCode::Stack_fence_too_high);
  }
#if STACKSIZE_LEFT_BEFORE_NATIVE_CALL
  if ((pToNum(stackFence) + static_cast<uintptr_t>(STACKSIZE_LEFT_BEFORE_NATIVE_CALL)) <= pToNum(stackFence)) {
    throw RuntimeError(ErrorCode::Cannot_keep_STACKSIZE_LEFT_BEFORE_NATIVE_CALL_free_before_native_call__Stack_fence_too_high_);
  }
#endif
  writeToPtr<uint8_t *>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::stackFence), pAddI(pCast<uint8_t const *>(stackFence), 64));
}
#endif

#if BUILTIN_FUNCTIONS
// Link a memory buffer so boardnet messages and other objects do not need to be
// copied into the linear memory. Do not forget to unlink the memory again!
bool Runtime::linkMemory(uint8_t const *base, uint32_t length) const VB_NOEXCEPT {
#ifdef JIT_TARGET_TRICORE

  if ((pToNum(base) % 2U) != 0U) {
    return false;
  }
  if (length >= (1_U32 << 30_U32)) {
    return false;
  }
#endif // JIT_TARGET_TRICORE

  if (length == 0U) {
    base = nullptr;
  } else if (base == nullptr) {
    length = 0U;
  } else {
    static_cast<void>(0);
  }

  writeToPtr<uint8_t const *>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::linkedMemPtr), base);
  writeToPtr<uint32_t>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::linkedMemLen), length);
  return true;
}

void Runtime::unlinkMemory() const VB_NOEXCEPT {
  static_cast<void>(linkMemory(nullptr, 0U));
}

void Runtime::clearTraceBuffer() VB_NOEXCEPT {
  writeToPtr<uint32_t *>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::traceBufferPtr), nullptr);
}

// coverity[autosar_cpp14_a8_4_7_violation]
void Runtime::setTraceBuffer(Span<uint32_t> buffer) VB_NOEXCEPT {
#if CXX_TARGET == ISA_AARCH64
  assert((pToNum(buffer.data()) % 16U) == 0U && "arm stp ldp requires 16 byte alignment");
#endif
  assert(buffer.size() <= UINT32_MAX && buffer.size() >= 2U);
  buffer[0] = static_cast<uint32_t>((buffer.size() >> 1U) - 1U);
  writeToPtr<uint32_t *>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::traceBufferPtr), &buffer[2]);
}

#endif // BUILTIN_FUNCTIONS

void Runtime::resetStacktraceAndDebugRecords() const VB_NOEXCEPT {
  uint32_t const stacktraceRecordCount{binaryModule_.getStacktraceEntryCount()};
  if (stacktraceRecordCount == 0U) {
    return;
  }

  // Get base of array and reset elements to 0xFFFF'FFFF
  uint8_t *const arrayBase{pSubI(getLinearMemoryBase(), Basedata::FromEnd::getStacktraceArrayBase(stacktraceRecordCount))};
  static_cast<void>(std::memset(arrayBase, 0xFF, static_cast<size_t>(stacktraceRecordCount) * sizeof(uint32_t)));

  // Reset last frame ptr
  writeToPtr<uint64_t>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::lastFrameRefPtr), 0_U64);
}

void Runtime::iterateStacktraceRecords(FunctionRef<void(uint32_t fncIndex)> const &lambda) const {
  uint32_t const stacktraceRecordCount{binaryModule_.getStacktraceEntryCount()};
  uint8_t *const arrayBase{pSubI(getLinearMemoryBase(), static_cast<uint32_t>(Basedata::FromEnd::getStacktraceArrayBase(stacktraceRecordCount)))};

  for (uint32_t i{0U}; i < stacktraceRecordCount; i++) {
    uint32_t const fncIndex{readFromPtr<uint32_t>(pAddI(arrayBase, static_cast<uint32_t>(sizeof(uint32_t)) * i))};
    if (fncIndex == 0xFF'FF'FF'FFU) {
      break;
    }

    lambda(fncIndex);
  }
}

void Runtime::printStacktrace(ILogger &logger) const {
  uint8_t const *functionNamesSectionCursor{binaryModule_.getFunctionNameSectionEnd()};
  uint32_t const numFunctionNames{readNextValue<uint32_t>(&functionNamesSectionCursor)};
  uint8_t const *const functionNamesArray{functionNamesSectionCursor};

  uint32_t stacktraceCount{0U};
  iterateStacktraceRecords(FunctionRef<void(uint32_t)>([&logger, functionNamesArray, numFunctionNames, &stacktraceCount](uint32_t const fncIndex) {
    stacktraceCount++;

    uint8_t const *innerStepPtr{functionNamesArray};

    for (uint32_t i{0U}; i < numFunctionNames; i++) {
      static_cast<void>(i);
      uint32_t const nameFunctionIndex{readNextValue<uint32_t>(&innerStepPtr)};
      uint32_t const nameLength{readNextValue<uint32_t>(&innerStepPtr)};

      innerStepPtr = pSubI(innerStepPtr, roundUpToPow2(nameLength, 2U));

      if (nameFunctionIndex == fncIndex) {
        char const *const name{pCast<char const *>(innerStepPtr)};
        logger << "\tat ";
        static_cast<void>(logger << Span<const char>(name, nameLength));
        logger << " (wasm-function[" << fncIndex << "])" << &endStatement<LogLevel::LOGERROR>;
        return;
      }
    }

    // Fallback if there is no name
    logger << "\tat (wasm-function[" << fncIndex << "])" << &endStatement<LogLevel::LOGERROR>;
  }));

  if (stacktraceCount == 0U) {
    logger << "No stacktrace records found\n";
  }
}

void Runtime::updateRuntimeReference() const VB_NOEXCEPT {
  static_assert(sizeof(void *) <= 8, "Pointer too big");
  void const *const ptr{pCast<void const *>(this)};
  writeToPtr<void const *>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::runtimePtrOffset), ptr);
#if LINEAR_MEMORY_BOUNDS_CHECKS
  void const *const jobMemoryDataPtrPtr{pCast<void const *>(&this->jobMemory_.data_)};
  writeToPtr<void const *>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::jobMemoryDataPtrPtr), jobMemoryDataPtrPtr);
#endif
}

uint32_t Runtime::getLinearMemorySizeInPages() const VB_NOEXCEPT {
  return readFromPtr<uint32_t>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::linMemWasmSize));
}

#if LINEAR_MEMORY_BOUNDS_CHECKS
uint64_t Runtime::getMemoryUsage() const VB_NOEXCEPT {
  uint32_t const basedataLength{getBasedataLength()};
  uint32_t const linearMemoryLength{
      readFromPtr<uint32_t>(pAddI(getMemoryBase(), basedataLength - static_cast<uint32_t>(Basedata::FromEnd::actualLinMemByteSize)))};
  return static_cast<uint64_t>(linearMemoryLength) + basedataLength;
}

void Runtime::reallocShrinkToBasedataSize() {
  // New memory region must be at least of getBasedataLength() size to fit job basedata into. Use getBasedataLength to
  // size the memory region.
  uint32_t const basedataLength{getBasedataLength()};
  shrinkToSize(basedataLength);

  // Some active data has been removed, this is an unsafe procedure
  if (jobMemory_.size() < getMemoryUsage()) {
    writeToPtr<uint32_t>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::actualLinMemByteSize), jobMemory_.size() - basedataLength);
  }
}

void Runtime::reallocShrinkToActiveSize() {
  // Execute a reallocation with minimumLength = active size
  assert(getMemoryUsage() <= UINT32_MAX && "Memory usage too high");
  shrinkToSize(static_cast<uint32_t>(getMemoryUsage()));
}
#endif

// coverity[autosar_cpp14_a15_4_4_violation]
void Runtime::shrinkToSize(uint32_t const minimumLength) {
#if LINEAR_MEMORY_BOUNDS_CHECKS
  if ((minimumLength < jobMemory_.size()) && jobMemory_.hasExtensionRequest()) {
    uint32_t const metadataLength{getBasedataLength()};
    uint32_t const minMemLength{metadataLength + minimumLength};
    jobMemory_.extensionRequest(minMemLength);
    writeToPtr<uint32_t>(pSubI(pAddI(getMemoryBase(), metadataLength), Basedata::FromEnd::actualLinMemByteSize), minimumLength);
    if ((getMemoryBase() == nullptr) || (jobMemory_.size() < minimumLength)) {
      throw RuntimeError(ErrorCode::Memory_reallocation_failed);
    }
  }
#else
  static_cast<void>(shrinkLinearMemory(minimumLength));
#endif
}

#if !LINEAR_MEMORY_BOUNDS_CHECKS
Runtime::LandingPadFnc Runtime::prepareLandingPad(void (*const targetFnc)(), void *const originalReturnAddress) const VB_NOEXCEPT {
  static_assert(sizeof(void *) == 8, "Pointer size mismatch");
  writeToPtr<uintptr_t>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::landingPadTarget), pToNum(targetFnc));
  writeToPtr<uintptr_t>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::landingPadRet), pToNum(originalReturnAddress));

  uint8_t const *const pLandingPad{binaryModule_.getLandingPadOrMemoryExtendFncAddress()};
  uint8_t *const pNoncost{pRemoveConst(pLandingPad)};
  return pCast<LandingPadFnc>(pNoncost);
}
#endif

#if !LINEAR_MEMORY_BOUNDS_CHECKS
void Runtime::updateLinearMemorySizeForDebugger() const VB_NOEXCEPT {
  if (!binaryModule_.debugMode()) {
    return;
  }
  uint32_t const linearMemorySize{memoryUsageFunction_(getBasedataLength())};

  writeToPtr<uint32_t>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::actualLinMemByteSize), linearMemorySize);
}
#endif

void Runtime::setMemoryHelperPtr() const VB_NOEXCEPT {
  static_assert(sizeof(void (*)(void)) <= 8, "Function pointer too large");

#if LINEAR_MEMORY_BOUNDS_CHECKS
  void *const newPtr{pCast<void *>(&MemoryHelper::extensionRequest)};
#else
  void const *const newPtr{pCast<void *>(&MemoryHelper::notifyOfMemoryGrowth)};
#endif

  writeToPtr<void *>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::memoryHelperPtr), newPtr);
}

#if INTERRUPTION_REQUEST

void Runtime::requestInterruption(TrapCode const trapCode) const VB_NOEXCEPT {
  // Cannot use writeToPtr because NO_THREAD_SANITIZE will still complain about other functions called here
  // Can also not use memcpy here for the same reason (even though it's inlined most of the time)
  // Because of this we will simply write a single byte, since the uint32_t only needs to be != 0 (Writing a single byte
  // will not violate the strict aliasing rule)
  uint8_t *const ptr{pSubI(getLinearMemoryBase(), Basedata::FromEnd::statusFlags)};

  uint8_t const rawTrapCode{static_cast<uint8_t>(static_cast<uint32_t>(trapCode))};
  *ptr = rawTrapCode;
}
#endif

void Runtime::resetTrapInfo() const VB_NOEXCEPT {
#if INTERRUPTION_REQUEST
  resetStatusFlags();
#endif
#ifdef JIT_TARGET_TRICORE
  writeToPtr<uint32_t>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::trapHandlerPtr), 0_U32);
  writeToPtr<uint32_t>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::trapStackReentry), 0_U32);
#else
  writeToPtr<uint64_t>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::trapHandlerPtr), 0_U64);
  writeToPtr<uint64_t>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::trapStackReentry), 0_U64);
#endif
}

bool Runtime::hasActiveFrame() const VB_NOEXCEPT {
  // If trap stack reentry ptr is not zero, we are currently executing (runtime has active function frames)
  uintptr_t const trapReentryPtr{readFromPtr<uintptr_t>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::trapStackReentry))};
  return trapReentryPtr != 0U;
}

void Runtime::prepareForFunctionCall() const VB_NOEXCEPT {
  if (!hasActiveFrame()) {
    // Reset stack trace records if not currently executing
    resetStacktraceAndDebugRecords();

    updateRuntimeReference();
  }
}

TrapCode Runtime::demuxTrapCode(TrapCode const trapCode) const VB_NOEXCEPT {
#if BUILTIN_FUNCTIONS
  if (trapCode == TrapCode::LINKEDMEMORY_MUX) {
    uint8_t const *const linkedMemoryBase{readFromPtr<uint8_t const *>(pSubI(getLinearMemoryBase(), Basedata::FromEnd::linkedMemPtr))};
    if (linkedMemoryBase == nullptr) {
      return TrapCode::LINKEDMEMORY_NOTLINKED;
    }
    return TrapCode::LINKEDMEMORY_OUTOFBOUNDS;
  }
#endif

  return trapCode;
}

void Runtime::handleTrapCode(TrapCode const trapCode) const {
  if (trapCode != TrapCode::NONE) {
    resetTrapInfo();
    throw TrapException(demuxTrapCode(trapCode));
  }
}

uint8_t *Runtime::getMemoryBase() const VB_NOEXCEPT {
#if LINEAR_MEMORY_BOUNDS_CHECKS
  uint8_t *const res{jobMemory_.data()};
#else
  uint8_t *const res{jobMemoryStart_};
#endif
  return res;
}

uint8_t *Runtime::getLinearMemoryBase() const VB_NOEXCEPT {
  return pAddI(getMemoryBase(), getBasedataLength());
}

uint8_t *Runtime::getLinearMemoryRegion(uint32_t const offset, uint32_t const size) const {
  if (size != 0U) {
    uint64_t const maxAccessedByte{(static_cast<uint64_t>(offset) + size) - 1U};
#if LINEAR_MEMORY_BOUNDS_CHECKS
    uint32_t const metadataLength{getBasedataLength()};
    uint8_t *const extensionResult{MemoryHelper::extensionRequest(maxAccessedByte + 1U, metadataLength, pAddI(getMemoryBase(), metadataLength))};

    if (extensionResult == nullptr) {
      throw RuntimeError(ErrorCode::Could_not_extend_linear_memory);
    }
    if (extensionResult == numToP<uint8_t *>(~static_cast<uintptr_t>(0))) {
      throw RuntimeError(ErrorCode::Linear_memory_address_out_of_bounds);
    }
#else
    if (maxAccessedByte >= (static_cast<uint64_t>(getLinearMemorySizeInPages()) * WasmConstants::wasmPageSize)) {
      throw RuntimeError(ErrorCode::Linear_memory_address_out_of_bounds);
    }
    if (!probeLinearMemory(static_cast<uint32_t>(maxAccessedByte))) {
      throw RuntimeError(ErrorCode::Could_not_extend_linear_memory);
    }
#endif
  }
  return pAddI(getLinearMemoryBase(), offset);
}

void Runtime::updateBinaryModule(BinaryModule const &module) const VB_NOEXCEPT {
  writeToPtr<uintptr_t>(getMemoryBase(), pToNum(module.getEndAddress()));
}

RawModuleFunction Runtime::getRawExportedFunctionByName(Span<char const> const &name, Span<char const> const &signature) const {
  checkIsReady();
  RawModuleFunction const function{RawModuleFunction(*this, findExportedFunctionByName(name.data(), name.size()))};
  if (signature.size() != 0U) {
    function.info().validateSignatures(signature);
  }
  return function;
}

RawModuleFunction Runtime::getRawFunctionByExportedTableIndex(uint32_t const tableIndex, Span<char const> const &signature) const {
  checkIsReady();
  RawModuleFunction const function{RawModuleFunction(*this, findFunctionByExportedTableIndex(tableIndex))};
  function.info().validateSignatures(signature);
  return function;
}

uint32_t Runtime::findExportedFunctionByName(char const *const name, size_t nameLength) const {
  if (nameLength == SIZE_MAX) {
    nameLength = strlen_s(name, static_cast<size_t>(ImplementationLimits::maxStringLength));
  }

  uint8_t const *exportedFunctionCursor{binaryModule_.getExportedFunctionsEnd()};
  uint32_t const numExportedFunctions{readNextValue<uint32_t>(&exportedFunctionCursor)};

  for (uint32_t i{0U}; i < numExportedFunctions; i++) {
    uint32_t const fncIndex{readNextValue<uint32_t>(&exportedFunctionCursor)};
    static_cast<void>(fncIndex);

    uint32_t const exportNameLength{readNextValue<uint32_t>(&exportedFunctionCursor)};
    exportedFunctionCursor = pSubI(exportedFunctionCursor, roundUpToPow2(exportNameLength, 2U));
    char const *const exportName{pCast<char const *>(exportedFunctionCursor)};

    if ((exportNameLength == nameLength) && (std::strncmp(name, exportName, nameLength) == 0)) { //
      return static_cast<uint32_t>(pSubAddr(binaryModule_.getEndAddress(), exportedFunctionCursor));
    }

    uint32_t const signatureLength{readNextValue<uint32_t>(&exportedFunctionCursor)};
    exportedFunctionCursor = pSubI(exportedFunctionCursor, roundUpToPow2(signatureLength, 2U));

    uint32_t const functionCallWrapperSize{readNextValue<uint32_t>(&exportedFunctionCursor)};
    exportedFunctionCursor = pSubI(exportedFunctionCursor, roundUpToPow2(functionCallWrapperSize, 2U));
  }

  throw RuntimeError(ErrorCode::Function_not_found);
}

uint32_t Runtime::findFunctionByExportedTableIndex(uint32_t const tableIndex) const {
  uint32_t const *const tableEntryStart{vb::pCast<uint32_t const *>(binaryModule_.getTableEntryFunctionsStart())};

  if (tableIndex < binaryModule_.getTableSize()) {
    uint32_t const functionOffsetToStart{tableEntryStart[tableIndex]};
    if (functionOffsetToStart != 0xFF'FF'FF'FFU) {
      uint32_t const offsetToEnd{binaryModule_.offsetToEnd(functionOffsetToStart)};
      return offsetToEnd;
    }
  }

  throw RuntimeError(ErrorCode::Function_not_found);
}

uint32_t Runtime::findExportedGlobalByName(char const *const name, size_t nameLength) const {
  if (nameLength == SIZE_MAX) {
    nameLength = strlen_s(name, static_cast<size_t>(ImplementationLimits::maxStringLength));
  }

  uint8_t const *exportedGlobalCursor{binaryModule_.getExportedGlobalsSectionEnd()};
  uint32_t const numExportedGlobals{readNextValue<uint32_t>(&exportedGlobalCursor)};

  for (uint32_t i{0U}; i < numExportedGlobals; i++) {
    uint32_t const exportNameLength{readNextValue<uint32_t>(&exportedGlobalCursor)};
    exportedGlobalCursor = pSubI(exportedGlobalCursor, roundUpToPow2(exportNameLength, 2U));
    char const *const exportName{pCast<char const *>(exportedGlobalCursor)};

    if ((exportNameLength == nameLength) && (std::strncmp(name, exportName, nameLength) == 0)) { //
      return static_cast<uint32_t>(pSubAddr(binaryModule_.getEndAddress(), exportedGlobalCursor));
    }

    exportedGlobalCursor = pSubI(exportedGlobalCursor, 2U); // Padding
    bool const isMutable{readNextValue<bool>(&exportedGlobalCursor)};
    if (isMutable) {
      exportedGlobalCursor = pSubI(exportedGlobalCursor, 4U);
    } else {
      SignatureType const signatureType{readNextValue<SignatureType>(&exportedGlobalCursor)};
      exportedGlobalCursor =
          pSubI(exportedGlobalCursor, ((signatureType == SignatureType::I32) || (signatureType == SignatureType::F32)) ? 4_U32 : 8_U32);
    }
  }

  throw RuntimeError(ErrorCode::Global_not_found);
}

FunctionInfo::FunctionInfo(uint8_t const *const binaryModulePtr, uint32_t const binaryOffset) VB_NOEXCEPT : signature_{nullptr, 0U},
                                                                                                            fncPtr_{nullptr} {
  // FunctionCallWrapper | FunctionCallWrapperSize | Signature | SignatureLength
  uint8_t const *stepPtr{pSubI(binaryModulePtr, binaryOffset)};
  uint32_t const storedSignatureLength{readNextValue<uint32_t>(&stepPtr)};
  stepPtr = pSubI(stepPtr, roundUpToPow2(storedSignatureLength, 2U));
  char const *const storedSignature{pCast<char const *>(stepPtr)};

  signature_.reset(storedSignature, static_cast<size_t>(storedSignatureLength));

  uint32_t const functionCallWrapperSize{readNextValue<uint32_t>(&stepPtr)};
  stepPtr = pSubI(stepPtr, roundUpToPow2(functionCallWrapperSize, 2U));
  fncPtr_ = stepPtr;
}

void FunctionInfo::validateSignatures(Span<char const> const &expectedSignature) const VB_THROW {
  if (signature_.size() != expectedSignature.size()) {
    throw RuntimeError(ErrorCode::Function_signature_mismatch__signature_size_mismatch);
  }
  if (strncmp(signature_.data(), expectedSignature.data(), signature_.size()) != 0) {
    throw RuntimeError(ErrorCode::Function_signature_mismatch);
  }
}

} // namespace vb

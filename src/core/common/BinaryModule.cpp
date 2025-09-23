///
/// @file BinaryModule.cpp
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
#include <cstdint>

#include "BinaryModule.hpp"

#include "src/core/common/Span.hpp"
#include "src/core/common/VbExceptions.hpp"
#include "src/core/common/util.hpp"

namespace vb {
void BinaryModule::init(Span<uint8_t const> const &module) {
#ifdef JIT_TARGET_X86_64
  if ((pToNum(module.data()) % 16U) != 0U) {
    throw RuntimeError(ErrorCode::Module_memory_not_16_byte_aligned);
  }
#else
  if ((pToNum(module.data()) % 8U) != 0U) {
    throw RuntimeError(ErrorCode::Module_memory_not_8_byte_aligned);
  }
#endif
  startAddress_ = module.data();
  endAddress_ = pAddI(startAddress_, module.size());
  uint8_t const *stepPtr{endAddress_};
  static_assert(sizeof(uintptr_t) <= 8U, "Pointer datatype too big");
  // SECTION: More Info
  moduleBinaryLength_ = readNextValue<uint32_t>(&stepPtr); // OPBVMET3

  uint32_t const version{readNextValue<uint32_t>(&stepPtr)}; // OPBVER
  if (version != versionNumber) {
    throw RuntimeError(ErrorCode::Binary_module_version_not_supported);
  }

  uint32_t const stacktraceEntry{readNextValue<uint32_t>(&stepPtr)}; // OPBVMET2
  stacktraceEntryCount_ = stacktraceEntry & 0x7F'FF'FF'FFU;
  debugMode_ = (stacktraceEntry & 0x80'00'00'00U) != 0U;
  uint32_t const landingPadOffset{readNextValue<uint32_t>(&stepPtr)}; // OPBVMET1
  if (landingPadOffset != UINT32_MAX) {
    landingPadAddress_ = pSubI(stepPtr, landingPadOffset);
  } else {
    landingPadAddress_ = nullptr;
  }

  linkDataLength_ = readNextValue<uint32_t>(&stepPtr); // OPBVMET0;
  // Table Function Entry for C++ section
  uint32_t const numTableFunctionEntries{readNextValue<uint32_t>(&stepPtr)};                   // OBBTE0
  stepPtr = pSubI(stepPtr, numTableFunctionEntries * static_cast<uint32_t>(sizeof(uint32_t))); // Skip table function entries (OBBTE1)
  tableEntryFunctionsStart_ = stepPtr;

  // Table section
  uint32_t const numTableEntries{readNextValue<uint32_t>(&stepPtr)}; // OPBVT2

  // GCOVR_EXCL_START
  assert((numTableFunctionEntries == numTableEntries) && "Mismatch of number of table entries");
  // GCOVR_EXCL_STOP
  tableSize_ = numTableEntries;

  stepPtr = pSubI(stepPtr, numTableEntries * (4U + 4U)); // Skip table entries (OPBVT0 + OPBVT1)
  tableStart_ = stepPtr;
  // Link Status section
  uint32_t const numLinkStatusEntries{readNextValue<uint32_t>(&stepPtr)};      // OPBILS3
  uint32_t const linkStatusPadding{deltaToNextPow2(numLinkStatusEntries, 2U)}; // OPBILS2
  stepPtr = pSubI(stepPtr, linkStatusPadding);
  stepPtr = pSubI(stepPtr, numLinkStatusEntries); // Skip link status entries (OPBILS1)
  linkStatusStart_ = stepPtr;

  // SECTION: Exported Functions
  uint32_t const exportedFunctionsSectionSize{readNextValue<uint32_t>(&stepPtr)}; // OPBVEF12
  exportedFunctionsEnd_ = stepPtr;
  // Skip exported functions (OPBVEF0, OPBVEF1, OPBVEF2, OPBVEF3, OPBVEF4,
  // OPBVEF5, OPBVEF6, OPBVEF7, OPBVEF8, OPBVEF9, OPBVEF12,
  // OPBVEF13)
  stepPtr = pSubI(stepPtr, exportedFunctionsSectionSize);

  // SECTION: Exported Globals
  uint32_t const exportedGlobalsSectionSize{readNextValue<uint32_t>(&stepPtr)}; // OPBVEG8

  // Skip exported globals (OPBVMEM0A, OPBVMEM0B, OPBVEG0, OPBVEG1, OPBVEG2,
  exportedGlobalsEnd_ = stepPtr;
  // OPBVEG3, OPBVEG4, OPBVEG5, OPBVEG6, OPBVEG7)
  stepPtr = pSubI(stepPtr, exportedGlobalsSectionSize);

  // SECTION: Memory
  initialMemorySize_ = readNextValue<uint32_t>(&stepPtr); // OPBVMEM0

  // SECTION: Dynamically Imported Functions
  uint32_t const dynamicallyImportedFunctionsSectionSize{readNextValue<uint32_t>(&stepPtr)}; // OPBVIF11
  dynamicallyImportedFunctionsSectionEnd_ = stepPtr;
  stepPtr = pSubI(stepPtr, dynamicallyImportedFunctionsSectionSize);

  // SECTION: Mutable Native Wasm Globals
  uint32_t const mutableGlobalsSectionSize{readNextValue<uint32_t>(&stepPtr)}; // OPBVNG5;
  mutableGlobalsSectionEnd_ = stepPtr;
  stepPtr = pSubI(stepPtr, mutableGlobalsSectionSize);

  // SECTION: Start Function
  uint32_t const startFunctionSectionSize{readNextValue<uint32_t>(&stepPtr)}; // OPBVSF6, if it has start function, save current position
  // Skip start function (OPBVSF5, OPBVSF4, OPBSF3, OPBSF2, OPBSF1, OOPBSF0)
  startFunctionBinaryOffset_ = (startFunctionSectionSize > 0U) ? static_cast<uint32_t>(pSubAddr(endAddress_, stepPtr)) : UINT32_MAX;
  stepPtr = pSubI(stepPtr, startFunctionSectionSize);

  // SECTION: Function Names
  uint32_t const functionNameSectionSize{readNextValue<uint32_t>(&stepPtr)};
  functionNameSectionEnd_ = stepPtr;
  stepPtr = pSubI(stepPtr, functionNameSectionSize); // Skip function name section

  // SECTION: Data

  numDataSegments_ = readNextValue<uint32_t>(&stepPtr); // OPBVLM4
  dataSegmentsEnd_ = stepPtr;
}
} // namespace vb

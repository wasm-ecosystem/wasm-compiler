///
/// @file MemoryDumpAPI.cpp
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
#ifndef EXTENSIONS_MEMORYDUMPAPI_CPP
#define EXTENSIONS_MEMORYDUMPAPI_CPP

#include <array>
#include <cassert>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <ios>
#include <string>

#include "extensions/MemoryDumpAPI.hpp"

#include "src/WasmModule/WasmModule.hpp"
#include "src/core/common/WasmConstants.hpp"

namespace vb {
namespace extension {

void MemoryDumpExtension::dumpMemoryRegion(uint32_t const memoryPtrOffset, uint32_t const size, void *const ctx) {
  WasmModule *const pWasmModule{pCast<WasmModule *>(ctx)};
  assert(pWasmModule);

  void *const memory = static_cast<void *>(pWasmModule->getLinearMemoryRegion(memoryPtrOffset, size));
  std::string const filePath = std::string(static_cast<char *>(memory), size);

  // Retrieve metadata globals by name
  uint32_t const dataEnd = pWasmModule->getExportedGlobalByName<uint32_t>("__data_end").getValue();
  uint32_t const heapBase = pWasmModule->getExportedGlobalByName<uint32_t>("__heap_base").getValue();
  uint32_t const stackPointer = pWasmModule->getExportedGlobalByName<uint32_t>("__stack_pointer").getValue();

  uint32_t const linearMemorySizeInPages = pWasmModule->getLinearMemorySizeInPages();
  uint32_t const linearMemorySize = linearMemorySizeInPages * WasmConstants::wasmPageSize;
  uint8_t *const buf = pWasmModule->getLinearMemoryRegion(0, linearMemorySize);

  std::ofstream outFile(filePath.c_str(), std::ios::out | std::ios::binary | std::ios::trunc);
  if (!outFile.is_open()) {
    return;
  }

/*
Dump file format:
┌──────────────────────────────────────────────────────────┐
│  Magic: "ASHD" (4 bytes, 0x41 0x53 0x48 0x44)            │
│  Version: u32 (1)                                        │
│  dataEnd: u32                                            │
│  heapBase: u32                                           │
│  stackPointer: u32                                       │
│  Reserved: 4 bytes (zero-filled, future use)             │
├──────────────────────────────────────────────────────────┤
│  Raw linear memory (file_size - 24 bytes)                │
*/

  constexpr std::array<char, 4> magic{{'A', 'S', 'H', 'D'}};
  outFile.write(magic.data(), magic.size());

  constexpr uint32_t version{1U};
  outFile.write(pCast<char const *>(&version), sizeof(version));

  outFile.write(pCast<char const *>(&dataEnd), sizeof(dataEnd));
  outFile.write(pCast<char const *>(&heapBase), sizeof(heapBase));
  outFile.write(pCast<char const *>(&stackPointer), sizeof(stackPointer));

  constexpr std::array<char, 4> reserved{{}};
  outFile.write(reserved.data(), reserved.size());

  // Raw linear memory
  outFile.write(static_cast<char *>(static_cast<void *>(buf)), linearMemorySize);
}

} // namespace extension
} // namespace vb

#endif // EXTENSIONS_MEMORYDUMPAPI_CPP

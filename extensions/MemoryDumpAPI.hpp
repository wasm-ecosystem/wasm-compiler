///
/// @file MemoryDumpAPI.hpp
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
#ifndef EXTENSIONS_MEMORYDUMPAPI_HPP
#define EXTENSIONS_MEMORYDUMPAPI_HPP

#include <cstdint>

namespace vb {
namespace extension {

///
/// @brief Memory dump extension
///
/// This extension provides import API for WebAssembly modules to dump linear memory content.
/// It allows WebAssembly code to request memory dumps at specific offsets and sizes,
/// which can be useful for debugging and analysis.
///
class MemoryDumpExtension final {
public:
  MemoryDumpExtension() = delete; ///< Not instantiable, static-only class

  /// @brief Dump memory region to the configured output stream
  /// @param memoryPtrOffset Offset in linear memory
  /// @param size Size of memory region to dump
  static void dumpMemoryRegion(uint32_t const memoryPtrOffset, uint32_t const size, void *const ctx);
};

} // namespace extension
} // namespace vb

#endif // EXTENSIONS_MEMORYDUMPAPI_HPP

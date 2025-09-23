///
/// @file WasmImportExportType.hpp
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
#ifndef WASMIMPORTEXPORTTYPE_HPP
#define WASMIMPORTEXPORTTYPE_HPP

#include <cstdint>

namespace vb {

///
/// @brief WebAssembly import and export types
///
enum class WasmImportExportType : uint8_t { FUNC = 0x00, TABLE = 0x01, MEM = 0x02, GLOBAL = 0x03 };

} // namespace vb

#endif

///
/// @file Extension.hpp
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
#ifndef SRC_EXTENSIONS_EXTENSION_HPP
#define SRC_EXTENSIONS_EXTENSION_HPP

#include "src/core/runtime/Runtime.hpp"

namespace vb {
namespace extension {
/// @brief register a runtime with the tracing extension
/// @param runtime the runtime to register
void registerRuntime(Runtime &runtime);
/// @brief unregister a runtime from the tracing extension
/// @param runtime the runtime to unregister
void unregisterRuntime(Runtime &runtime);
/// @brief stop the tracing extension and write the collected data to file
void stop();

} // namespace extension
} // namespace vb

#endif // SRC_EXTENSIONS_EXTENSION_HPP

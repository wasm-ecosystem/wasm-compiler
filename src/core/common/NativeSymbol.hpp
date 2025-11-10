///
/// @file NativeSymbol.hpp
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
#ifndef NATIVESYMBOL_HPP
#define NATIVESYMBOL_HPP

#include <cstdint>

namespace vb {

///
/// @brief Reference of a native symbol (e.g. a C++ function) that can be linked (as an imported symbol) to a Wasm
/// module by the compiler
///
class NativeSymbol final {
public:
  ///
  /// @brief Denotes how the native symbol (e.g. a C++ function) should be linked by the compiler
  ///
  enum class Linkage : uint8_t { STATIC, DYNAMIC };

  ///
  /// @brief Denoting the type of linkage, i.e. whether it will be statically linked during compilation or dynamically
  /// linked during initialization of the Runtime
  ///
  /// This field will not be read by the runtime since it is assumed that the runtime will only be given the dynamically
  /// linked NativeSymbols. Setting this to DYNAMIC and passing it to the compiler will be equivalent to a "placeholder"
  /// and will tell the compiler that the final symbol will be passed to the runtime
  ///
  Linkage linkage = Linkage::DYNAMIC;

  ///
  /// @brief Name of the module as a pointer to a null-terminated string
  ///
  char const *moduleName = nullptr;

  ///
  /// @brief Name of the symbol as a pointer to a null-terminated string
  ///
  char const *symbol = nullptr;

  ///
  /// @brief Signature of the symbol (function) as a pointer to a null-terminated string, see also SignatureType; e.g.
  /// (iIfF)f
  ///
  char const *signature = nullptr;

  ///
  /// @brief The pointer to the native symbol (e.g. a function pointer if the symbol is a C++ function)
  ///
  /// Will not be read if the linkage is Linkage::DYNAMIC and it is passed to the compiler
  /// NOTE: The runtime will read this field irrespective of the Linkage
  ///
  void const *ptr = nullptr;

  ///
  /// @brief Denotes which version should import function used
  ///
  enum class ImportFnVersion : uint8_t { V1, V2 };
  ///
  /// @brief Whether this function is v2 imported function or v1
  ///
  ImportFnVersion importVersion = ImportFnVersion::V1;
};

} // namespace vb

#endif /* NATIVESYMBOL_H */

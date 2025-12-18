///
/// @file GlobalSymbol.hpp
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
#ifndef VB_GLOBAL_SYMBOL_HPP
#define VB_GLOBAL_SYMBOL_HPP

#include <cassert>
#include <cstdint>
#include <cstring>

#include "WasmType.hpp"
#include "util.hpp"

namespace vb {

///
/// @brief Class representing an imported global variable
///
class GlobalSymbol final {
public:
  ///
  /// @brief Create an GlobalSymbol from a signed 32-bit integer
  ///
  /// @param moduleName Name of the module as a pointer to a null-terminated string
  /// @param fieldName Name of the global field as a pointer to a null-terminated string
  /// @param value The signed 32-bit integer value
  /// @return GlobalSymbol instance
  ///
  inline static GlobalSymbol fromInt32(char const *const moduleName, char const *const fieldName, int32_t const value) VB_NOEXCEPT {
    ConstUnion storage{};
    storage.u32 = static_cast<uint32_t>(value);
    return GlobalSymbol{moduleName, fieldName, storage, WasmType::I32};
  }

  ///
  /// @brief Create an GlobalSymbol from an unsigned 32-bit integer
  ///
  /// @param moduleName Name of the module as a pointer to a null-terminated string
  /// @param fieldName Name of the global field as a pointer to a null-terminated string
  /// @param value The unsigned 32-bit integer value
  /// @return GlobalSymbol instance
  ///
  inline static GlobalSymbol fromUInt32(char const *const moduleName, char const *const fieldName, uint32_t const value) VB_NOEXCEPT {
    ConstUnion storage{};
    storage.u32 = value;
    return GlobalSymbol{moduleName, fieldName, storage, WasmType::I32};
  }

  ///
  /// @brief Create an GlobalSymbol from a signed 64-bit integer
  ///
  /// @param moduleName Name of the module as a pointer to a null-terminated string
  /// @param fieldName Name of the global field as a pointer to a null-terminated string
  /// @param value The signed 64-bit integer value
  /// @return GlobalSymbol instance
  ///
  inline static GlobalSymbol fromInt64(char const *const moduleName, char const *const fieldName, int64_t const value) VB_NOEXCEPT {
    ConstUnion storage{};
    storage.u64 = static_cast<uint64_t>(value);
    return GlobalSymbol{moduleName, fieldName, storage, WasmType::I64};
  }

  ///
  /// @brief Create an GlobalSymbol from an unsigned 64-bit integer
  ///
  /// @param moduleName Name of the module as a pointer to a null-terminated string
  /// @param fieldName Name of the global field as a pointer to a null-terminated string
  /// @param value The unsigned 64-bit integer value
  /// @return GlobalSymbol instance
  ///
  inline static GlobalSymbol fromUInt64(char const *const moduleName, char const *const fieldName, uint64_t const value) VB_NOEXCEPT {
    ConstUnion storage{};
    storage.u64 = value;
    return GlobalSymbol{moduleName, fieldName, storage, WasmType::I64};
  }

  ///
  /// @brief Create an GlobalSymbol from a 32-bit floating-point value
  ///
  /// @param moduleName Name of the module as a pointer to a null-terminated string
  /// @param fieldName Name of the global field as a pointer to a null-terminated string
  /// @param value The 32-bit floating-point value
  /// @return GlobalSymbol instance
  ///
  inline static GlobalSymbol fromFloat32(char const *const moduleName, char const *const fieldName, float const value) VB_NOEXCEPT {
    ConstUnion storage{};
    storage.f32 = value;
    return GlobalSymbol{moduleName, fieldName, storage, WasmType::F32};
  }

  ///
  /// @brief Create an GlobalSymbol from a 64-bit floating-point value
  ///
  /// @param moduleName Name of the module as a pointer to a null-terminated string
  /// @param fieldName Name of the global field as a pointer to a null-terminated string
  /// @param value The 64-bit floating-point value
  /// @return GlobalSymbol instance
  ///
  inline static GlobalSymbol fromFloat64(char const *const moduleName, char const *const fieldName, double const value) VB_NOEXCEPT {
    ConstUnion storage{};
    storage.f64 = value;
    return GlobalSymbol{moduleName, fieldName, storage, WasmType::F64};
  }

  ///
  /// @brief Get the module name of this imported global
  ///
  /// @return Pointer to a null-terminated string containing the module name
  ///
  inline char const *getModuleName() const VB_NOEXCEPT {
    return moduleName_;
  }

  ///
  /// @brief Get the field name of this imported global
  ///
  /// @return Pointer to a null-terminated string containing the field name
  ///
  inline char const *getFieldName() const VB_NOEXCEPT {
    return fieldName_;
  }

  ///
  /// @brief Get the WebAssembly type of this imported global
  ///
  /// @return The WasmType (I32, I64, F32, or F64)
  ///
  inline WasmType getType() const VB_NOEXCEPT {
    return type_;
  }

  ///
  /// @brief Get the value as an unsigned 32-bit integer
  ///
  /// @return The stored value interpreted as uint32_t
  ///
  inline uint32_t getUInt32() const VB_NOEXCEPT {
    assert(type_ == WasmType::I32 && "getUInt32 called on non-I32 type");
    return storage_.u32;
  }

  ///
  /// @brief Get the value as an unsigned 64-bit integer
  ///
  /// @return The stored value as uint64_t
  ///
  inline uint64_t getUInt64() const VB_NOEXCEPT {
    assert(type_ == WasmType::I64 && "getUInt64 called on non-I64 type");
    return storage_.u64;
  }

  ///
  /// @brief Get the value as a 32-bit floating-point number
  ///
  /// @return The stored bit pattern interpreted as float
  ///
  inline float getFloat32() const VB_NOEXCEPT {
    assert(type_ == WasmType::F32 && "getFloat32 called on non-F32 type");
    return storage_.f32;
  }

  ///
  /// @brief Get the value as a 64-bit floating-point number
  ///
  /// @return The stored bit pattern interpreted as double
  ///
  inline double getFloat64() const VB_NOEXCEPT {
    assert(type_ == WasmType::F64 && "getFloat64 called on non-F64 type");
    return storage_.f64;
  }

private:
  ///
  /// @brief Construct an GlobalSymbol with the specified module name, field name, storage value, and type
  ///
  /// @param moduleName Name of the module as a pointer to a null-terminated string
  /// @param fieldName Name of the global field as a pointer to a null-terminated string
  /// @param storage The ConstUnion storage value (may contain integer or floating-point values)
  /// @param type The WebAssembly type of the global variable
  ///
  inline GlobalSymbol(char const *const moduleName, char const *const fieldName, ConstUnion const storage, WasmType const type) VB_NOEXCEPT
      : moduleName_{moduleName},
        fieldName_{fieldName},
        storage_{storage},
        type_{type} {
  }

  ///
  /// @brief Name of the module as a pointer to a null-terminated string
  ///
  const char *moduleName_;

  ///
  /// @brief Name of the global field as a pointer to a null-terminated string
  ///
  const char *fieldName_;

  ///
  /// @brief Storage for the global's value (union that may represent integers or floating-point values)
  ///
  ConstUnion storage_;

  ///
  /// @brief The WebAssembly type of this global variable
  ///
  WasmType type_;
};

} // namespace vb

#endif /* GlobalSymbol */

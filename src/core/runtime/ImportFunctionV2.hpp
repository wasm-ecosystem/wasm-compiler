///
/// @file ImportFunctionV2.hpp
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
#ifndef IMPORT_FUNCTION_V2_HPP
#define IMPORT_FUNCTION_V2_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <initializer_list>
#include <tuple>
#include <type_traits>
#include <utility>

#include "src/config.hpp"
#include "src/core/common/NativeSymbol.hpp"
#include "src/core/common/function_traits.hpp"
#include "src/core/common/util.hpp"

///
/// @brief Helper trait to detect if a type is a std::tuple
/// @tparam T Type to check
///
template <typename T> struct is_std_tuple final : std::false_type {};

///
/// @brief Specialization for std::tuple types
/// @tparam Ts Template parameter pack for tuple element types
///
template <typename... Ts> struct is_std_tuple<std::tuple<Ts...>> final : std::true_type {};

///
/// @brief Check if a type is an allowed scalar type for V2 import functions
///
/// Allowed types are WebAssembly core numeric types: uint32_t, uint64_t, float, double.
/// References and cv-qualified types are not allowed.
///
/// @tparam T Type to check
///
template <typename T> struct is_allowed_scalar final {
  /// @brief Type T with cv-qualifiers and references removed
  using U = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
  /// @brief True if U is one of the allowed WASM scalar types and T is not a reference
  static constexpr bool value =
      (std::is_same<U, uint32_t>::value || std::is_same<U, uint64_t>::value || std::is_same<U, float>::value || std::is_same<U, double>::value) &&
      !std::is_reference<T>::value;
};

///
/// @brief Recursively check if all elements in a tuple are allowed scalar types
/// @tparam Tuple The tuple type to check
/// @tparam Index Current index being checked
/// @tparam N Total number of elements in the tuple
///
template <typename Tuple, std::size_t Index = 0, std::size_t N = std::tuple_size<Tuple>::value> struct tuple_elements_allowed final {
  /// @brief True if all elements from Index to N are allowed scalar types
  static constexpr bool value =
      is_allowed_scalar<typename std::tuple_element<Index, Tuple>::type>::value && tuple_elements_allowed<Tuple, Index + 1, N>::value;
};

///
/// @brief Specialization for tuple_elements_allowed when all elements have been checked
/// @tparam Tuple The tuple type
/// @tparam N Total number of elements
///
template <typename Tuple, std::size_t N> struct tuple_elements_allowed<Tuple, N, N> final {
  /// @brief Base case: all elements have been checked successfully
  static constexpr bool value = true;
};

///
/// @brief Template class for V2 import functions with strongly-typed parameters and return values
///
/// This class provides type-safe wrappers for V2 import functions, handling parameter and return value
/// marshalling between WebAssembly and native code. V2 import functions support multiple return values
/// and use a different calling convention than V1 imports.
///
/// @tparam ParamsTuple A std::tuple containing parameter types (must be uint32_t, uint64_t, float, or double)
/// @tparam RetsTuple A std::tuple containing return types (must be uint32_t, uint64_t, float, or double)
///
template <typename ParamsTuple, typename RetsTuple> class ImportFunctionV2 {
  static_assert(is_std_tuple<ParamsTuple>::value, "ParamsTuple must be a std::tuple");
  static_assert(is_std_tuple<RetsTuple>::value, "RetsTuple must be a std::tuple");
  static_assert(tuple_elements_allowed<ParamsTuple>::value, "ParamsTuple elements must be one of {uint32_t,uint64_t,float,double} (no refs/cv)");
  static_assert(tuple_elements_allowed<RetsTuple>::value, "RetsTuple elements must be one of {uint32_t,uint64_t,float,double} (no refs/cv)");

public:
  /// @brief Type alias for the parameters tuple
  using Params = ParamsTuple;
  /// @brief Type alias for the returns tuple
  using Returns = RetsTuple;
  /// @brief Function pointer type for V2 API functions
  using ApiFnV2 = void (*)(void *, void *, void *);

  /// @brief Number of parameters in the function
  static constexpr std::size_t paramCount = std::tuple_size<Params>::value;
  /// @brief Number of return values in the function
  static constexpr std::size_t retCount = std::tuple_size<Returns>::value;

  /// @brief Get the parameter type at a specific index
  /// @tparam Index The index of the parameter
  template <std::size_t Index> using ParamType = typename std::tuple_element<Index, Params>::type;

  /// @brief Get the return type at a specific index
  /// @tparam Index The index of the return value
  template <std::size_t Index> using ReturnType = typename std::tuple_element<Index, Returns>::type;

  /// @brief Generate a NativeSymbol for this import function
  /// @param module The module name
  /// @param symbol The symbol name
  /// @param linkType The linkage type (static or dynamic)
  /// @param fn The function pointer
  /// @return A NativeSymbol structure
  static vb::NativeSymbol generateNativeSymbol(char const *module, char const *symbol, vb::NativeSymbol::Linkage linkType, ApiFnV2 fn) VB_NOEXCEPT {
    return vb::NativeSymbol{
        linkType, module, symbol, buildSignature(), vb::pCast<void *>(fn), vb::NativeSymbol::ImportFnVersion::V2,
    };
  }

  /// @brief Get a parameter value from the parameters base pointer
  /// @tparam Index The index of the parameter to retrieve
  /// @param paramsBase Pointer to the base of the parameters array
  /// @return The parameter value at the specified index
  template <std::size_t Index> static ParamType<Index> getParam(void *paramsBase) VB_NOEXCEPT {
    static_assert(Index < paramCount, "getParam index out of range");
    using T = ParamType<Index>;
    constexpr size_t sizeToMove{sizeof(T)};
    static_assert(sizeToMove == 8U || sizeToMove == 4U, "must");
    uint8_t const *const base = vb::pCast<uint8_t const *const>(paramsBase);
    uint8_t const *const slot = base + (Index * 8U);
    T value{};
    std::memcpy(&value, slot, sizeToMove);
    return value;
  }

  /// @brief Set a return value in the results base pointer
  /// @tparam Index The index of the return value to set
  /// @param resultsBase Pointer to the base of the results array
  /// @param value The value to set at the specified index
  template <std::size_t Index> static void setRet(void *resultsBase, ReturnType<Index> value) VB_NOEXCEPT {
    static_assert(Index < retCount, "setRes index out of range");
    using T = ReturnType<Index>;
    constexpr size_t sizeToMove{sizeof(T)};
    static_assert(sizeToMove == 8U || sizeToMove == 4U, "must");
    uint8_t *const base = vb::pCast<uint8_t *const>(resultsBase);
    uint8_t *const slot = base + (Index * 8U);
    std::memset(slot, 0, 8U);
    std::memcpy(slot, &value, sizeToMove);
  }

private:
  /// @brief Size of the signature array (format: "(" + params + ")" + rets + "\0")
  static constexpr std::size_t signatureSize = 1 + paramCount + 1 + retCount + 1;
  /// @brief Static array containing the function signature for WASM
  static std::array<char, signatureSize> signature;
  /// @brief Build the WASM function signature string
  /// @return Pointer to the signature string in the format "(params)rets"
  static const char *buildSignature() VB_NOEXCEPT {
    signature[0] = '(';
    fillParams(std::make_index_sequence<paramCount>{});
    signature[1 + paramCount] = ')';
    fillRets(std::make_index_sequence<retCount>{});
    signature[signatureSize - 1] = '\0';
    return signature.data();
  }

  /// @brief Fill the parameter types into the signature string
  /// @tparam Is Parameter pack of indices
  template <std::size_t... Is> static void fillParams(std::index_sequence<Is...> /*idxs*/) VB_NOEXCEPT {
    (void)std::initializer_list<int>{
        (signature[1 + Is] =
             vb::TypeToSignature<typename std::remove_cv<typename std::remove_reference<ParamType<Is>>::type>::type>::getSignatureChar(),
         0)...};
  }

  /// @brief Fill the return types into the signature string
  /// @tparam Js Parameter pack of indices
  template <std::size_t... Js> static void fillRets(std::index_sequence<Js...> /*idxs*/) VB_NOEXCEPT {
    (void)std::initializer_list<int>{
        (signature[1 + paramCount + 1 + Js] =
             vb::TypeToSignature<typename std::remove_cv<typename std::remove_reference<ReturnType<Js>>::type>::type>::getSignatureChar(),
         0)...};
  }
};

/// @brief Static member definition for the signature array
template <typename ParamsTuple, typename RetsTuple>
std::array<char, ImportFunctionV2<ParamsTuple, RetsTuple>::signatureSize> ImportFunctionV2<ParamsTuple, RetsTuple>::signature = {};

#endif // IMPORT_FUNCTION_V2_HPP

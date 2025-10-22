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

// Helper trait: detect std::tuple
template <typename T> struct is_std_tuple final : std::false_type {};
template <typename... Ts> struct is_std_tuple<std::tuple<Ts...>> final : std::true_type {};

// Check allowed scalar (wasm core numeric types here restricted to unsigned for ints)
template <typename T> struct is_allowed_scalar final {
  using U = typename std::remove_cv<typename std::remove_reference<T>::type>::type;
  static constexpr bool value =
      (std::is_same<U, uint32_t>::value || std::is_same<U, uint64_t>::value || std::is_same<U, float>::value || std::is_same<U, double>::value) &&
      !std::is_reference<T>::value;
};

template <typename Tuple, std::size_t Index = 0, std::size_t N = std::tuple_size<Tuple>::value> struct tuple_elements_allowed final {
  static constexpr bool value =
      is_allowed_scalar<typename std::tuple_element<Index, Tuple>::type>::value && tuple_elements_allowed<Tuple, Index + 1, N>::value;
};
template <typename Tuple, std::size_t N> struct tuple_elements_allowed<Tuple, N, N> final {
  static constexpr bool value = true;
};

template <typename ParamsTuple, typename RetsTuple> class ImportFunctionV2 {
  static_assert(is_std_tuple<ParamsTuple>::value, "ParamsTuple must be a std::tuple");
  static_assert(is_std_tuple<RetsTuple>::value, "RetsTuple must be a std::tuple");
  static_assert(tuple_elements_allowed<ParamsTuple>::value, "ParamsTuple elements must be one of {uint32_t,uint64_t,float,double} (no refs/cv)");
  static_assert(tuple_elements_allowed<RetsTuple>::value, "RetsTuple elements must be one of {uint32_t,uint64_t,float,double} (no refs/cv)");

public:
  using Params = ParamsTuple;
  using Returns = RetsTuple;
  using ApiFnV2 = void (*)(void *, void *, void *);

  static constexpr std::size_t paramCount = std::tuple_size<Params>::value;
  static constexpr std::size_t retCount = std::tuple_size<Returns>::value;

  template <std::size_t Index> using ParamType = typename std::tuple_element<Index, Params>::type;
  template <std::size_t Index> using ReturnType = typename std::tuple_element<Index, Returns>::type;

  static vb::NativeSymbol generateNativeSymbol(char const *module, char const *symbol, vb::NativeSymbol::Linkage linkType, ApiFnV2 fn) VB_NOEXCEPT {
    return vb::NativeSymbol{
        linkType, module, symbol, buildSignature(), vb::pCast<void *>(fn), vb::NativeSymbol::ImportFnVersion::V2,
    };
  }

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
  static constexpr std::size_t signatureSize = 1 + paramCount + 1 + retCount + 1;
  static std::array<char, signatureSize> signature;
  static const char *buildSignature() VB_NOEXCEPT {
    signature[0] = '(';
    fillParams(std::make_index_sequence<paramCount>{});
    signature[1 + paramCount] = ')';
    fillRets(std::make_index_sequence<retCount>{});
    signature[signatureSize - 1] = '\0';
    return signature.data();
  }

  template <std::size_t... Is> static void fillParams(std::index_sequence<Is...> /*idxs*/) VB_NOEXCEPT {
    (void)std::initializer_list<int>{
        (signature[1 + Is] =
             vb::TypeToSignature<typename std::remove_cv<typename std::remove_reference<ParamType<Is>>::type>::type>::getSignatureChar(),
         0)...};
  }
  template <std::size_t... Js> static void fillRets(std::index_sequence<Js...> /*idxs*/) VB_NOEXCEPT {
    (void)std::initializer_list<int>{
        (signature[1 + paramCount + 1 + Js] =
             vb::TypeToSignature<typename std::remove_cv<typename std::remove_reference<ReturnType<Js>>::type>::type>::getSignatureChar(),
         0)...};
  }
};

template <typename ParamsTuple, typename RetsTuple>
std::array<char, ImportFunctionV2<ParamsTuple, RetsTuple>::signatureSize> ImportFunctionV2<ParamsTuple, RetsTuple>::signature = {};

#endif // IMPORT_FUNCTION_V2_HPP

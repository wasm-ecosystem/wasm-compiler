///
/// @file function_traits.hpp
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
#ifndef FUNCTION_TRAITS_HPP
#define FUNCTION_TRAITS_HPP

#include <array>
#include <cstddef>
#include <cstdint>
#include <tuple>

#include "SignatureType.hpp"

#include "src/config.hpp"

namespace vb {

///
/// @brief Type with removed VB_NOEXCEPT
///
/// @tparam T Resulting type
template <typename T> struct remove_noexcept final {
  using type = T; ///< Type
};

///
/// @brief Types with removed VB_NOEXCEPT
///
/// @tparam R First type
/// @tparam P Rest of the types
template <typename R, typename... P> struct remove_noexcept<R(P...) VB_NOEXCEPT> {
  using type = R(P...); ///< Type
};

///
/// @brief Type with removed VB_NOEXCEPT
///
/// @tparam T Resulting type
template <typename T> using remove_noexcept_t = typename remove_noexcept<T>::type;

template <typename C> struct TypeToSignature;

/// @brief Convert a uint32_t to its corresponding SignatureType
template <> struct TypeToSignature<uint32_t> {
  /// @brief Convert a uint32_t to its corresponding SignatureType
  static constexpr char getSignatureChar() VB_NOEXCEPT {
    return static_cast<char>(SignatureType::I32);
  }
};

/// @brief Convert an int32_t to its corresponding SignatureType
template <> struct TypeToSignature<int32_t> {
  /// @brief Convert an int32_t to its corresponding SignatureType
  static constexpr char getSignatureChar() VB_NOEXCEPT {
    return static_cast<char>(SignatureType::I32);
  }
};

/// @brief Convert a uint64_t to its corresponding SignatureType
template <> struct TypeToSignature<uint64_t> {
  /// @brief Convert a uint64_t to its corresponding SignatureType
  static constexpr char getSignatureChar() VB_NOEXCEPT {
    return static_cast<char>(SignatureType::I64);
  }
};

/// @brief Convert an int64_t to its corresponding SignatureType
template <> struct TypeToSignature<int64_t> {
  /// @brief Convert an int64_t to its corresponding SignatureType
  static constexpr char getSignatureChar() VB_NOEXCEPT {
    return static_cast<char>(SignatureType::I64);
  }
};

/// @brief Convert a float to its corresponding SignatureType
template <> struct TypeToSignature<float> {
  /// @brief Convert a float to its corresponding SignatureType
  static constexpr char getSignatureChar() VB_NOEXCEPT {
    return static_cast<char>(SignatureType::F32);
  }
};

/// @brief Convert an double to its corresponding SignatureType
template <> struct TypeToSignature<double> {
  /// @brief Convert an double to its corresponding SignatureType
  static constexpr char getSignatureChar() VB_NOEXCEPT {
    return static_cast<char>(SignatureType::F64);
  }
};

/// @brief Convert a double to its corresponding SignatureType (none)
template <> struct TypeToSignature<void> {
  /// @brief Convert a double to its corresponding SignatureType (none)
  static constexpr char getSignatureChar() VB_NOEXCEPT {
    return static_cast<char>(0);
  }
};

template <typename> struct function_traits;

///
/// @brief Signature serializer
///
/// @tparam ReturnType Return type of the function signature
/// @tparam Arguments Arguments of the function signature
template <typename ReturnType, typename... Arguments> struct function_traits<ReturnType(Arguments...)> {
  template <std::size_t Index> using argument = typename std::tuple_element<Index, std::tuple<Arguments...>>::type; ///< Argument type

  static constexpr size_t numCppParams{sizeof...(Arguments)}; ///< Number of C++ parameters
  static_assert(numCppParams >= 1U, "Function signature must have at least one parameter (the context pointer)");
  static_assert(std::is_pointer<argument<numCppParams - 1>>::value, "Last parameter must be a pointer type");
  ///
  /// @brief Number of arguments for this function signature
  ///
  static constexpr std::size_t arity = (numCppParams - 1U);

  ///
  /// @brief Create a corresponding C-style string signature
  ///
  /// @return const char* String signature corresponding to the function type
  static char const *getSignature() VB_NOEXCEPT {
    signature[0] = static_cast<char>(SignatureType::PARAMSTART);
    signature[arity + 1U] = static_cast<char>(SignatureType::PARAMEND);
    signature[arity + 2U] = getSignatureChar<ReturnType>();
    signature[arity + 3U] = 0;
    fillSignature<0U>();

    char const *signaturePtr;
    signaturePtr = signature.data();

    return signaturePtr;
  }

private:
  ///
  /// @brief Array to store the signature
  ///
  static std::array<char, arity + 3U + 1U> signature;

  ///
  /// @brief Fill the signature within the number of arguments
  ///
  /// @tparam index
  template <uint32_t index> static inline void fillSignatureImpl(std::integral_constant<bool, true> /*unused*/) VB_NOEXCEPT {
    signature[index + 1U] = getSignatureChar<argument<index>>();
    fillSignature<index + 1U>();
  }

  ///
  /// @brief Fill the signature beyond the number of arguments
  ///
  /// @tparam index
  template <uint32_t index> static inline void fillSignatureImpl(std::integral_constant<bool, false> /*unused*/) VB_NOEXCEPT {
  }

  ///
  /// @brief Fill the signature
  ///
  /// @tparam index
  template <uint32_t index = 0> static inline void fillSignature() VB_NOEXCEPT {
    fillSignatureImpl<index>(std::integral_constant<bool, (index < arity)>{});
  }

  ///
  /// @brief Get the corresponding signature char for a given datatype
  ///
  /// @tparam T basic numeric type
  /// @return char Corresponding signature char
  template <typename T> static constexpr char getSignatureChar(std::integral_constant<bool, false> /*unused*/) VB_NOEXCEPT {
    static_assert(std::is_same<T, int32_t>::value || std::is_same<T, uint32_t>::value || std::is_same<T, int64_t>::value ||
                      std::is_same<T, uint64_t>::value || std::is_same<T, float>::value || std::is_same<T, double>::value ||
                      std::is_same<T, void>::value,
                  "Argument type for linked function can only be i32, i64, f32, f64 and void");
    return TypeToSignature<T>::getSignatureChar();
  }

  ///
  /// @brief Get the corresponding signature char for a given datatype
  ///
  /// @tparam T Enum type
  /// @return char Corresponding signature char
  template <typename T> static constexpr char getSignatureChar(std::integral_constant<bool, true> /*unused*/) VB_NOEXCEPT {
    using UnderlyingType = typename std::underlying_type<T>::type;
    return getSignatureChar<UnderlyingType>(std::integral_constant<bool, false>());
  }

  ///
  /// @brief Get the corresponding signature char for a given datatype
  ///
  /// @tparam T Datatype
  /// @return char Corresponding signature char
  template <typename T> static constexpr char getSignatureChar() VB_NOEXCEPT {
    return getSignatureChar<T>(std::integral_constant<bool, std::is_enum<T>::value>());
  }
};

/// @brief see signature
template <typename ReturnType, typename... Arguments>
std::array<char, function_traits<ReturnType(Arguments...)>::arity + 3U + 1U> function_traits<ReturnType(Arguments...)>::signature{};
} // namespace vb

#define STATIC_LINK(moduleName, symbolName, fnc)                                                                                                     \
  vb::NativeSymbol {                                                                                                                                 \
    vb::NativeSymbol::Linkage::STATIC, (moduleName), (symbolName), vb::function_traits<vb::remove_noexcept_t<decltype(fnc)>>::getSignature(),        \
        vb::pCast<void *>(fnc)                                                                                                                       \
  }

#define DYNAMIC_LINK(moduleName, symbolName, fnc)                                                                                                    \
  vb::NativeSymbol {                                                                                                                                 \
    vb::NativeSymbol::Linkage::DYNAMIC, (moduleName), (symbolName), vb::function_traits<vb::remove_noexcept_t<decltype(fnc)>>::getSignature(),       \
        vb::pCast<void *>(fnc),                                                                                                                      \
  }
#endif

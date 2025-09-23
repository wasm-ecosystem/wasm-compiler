///
/// @file Span.hpp
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
#ifndef SPAN_HPP
#define SPAN_HPP

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>

#include "util.hpp"

#include "src/config.hpp"

namespace vb {

///
/// @brief A view onto a region of memory consisting of a base pointer and the length of the region
///
/// @tparam T Type of the array the span references
// coverity[autosar_cpp14_a14_1_1_violation]
template <typename T> class Span {
public:
  ///
  /// @brief Construct a new empty Span
  ///
  Span() VB_NOEXCEPT : Span{nullptr, 0U} {
  }

  ///
  /// @brief Construct a new Span
  ///
  /// @param data Pointer to the start of the array
  /// @param size Size of the array
  Span(T *const data, size_t const size) VB_NOEXCEPT : data_(data), size_(size) {
  }

  ///
  /// @brief Construct a new Span from an std::array
  ///
  /// @tparam N Number of array elements
  /// @param array Input array
  template <size_t N>
  explicit Span(std::array<typename std::add_const<T>::type, N> const &array) VB_NOEXCEPT : data_(array.data()), size_(array.size()) {
  }

  ///
  /// @brief Construct a new Span from an std::array
  ///
  /// @tparam N Number of array elements
  /// @param array Input array
  template <size_t N>
  explicit Span(std::array<typename std::remove_const<T>::type, N> const &array) VB_NOEXCEPT : data_(array.data()), size_(array.size()) {
  }

  ///
  /// @brief Get the base of the span
  ///
  /// @return uint8_t* Pointer to the start of span
  inline T *data() const VB_NOEXCEPT {
    T *const res{data_};
    return res;
  }

  ///
  /// @brief Get the length of the span
  ///
  /// @return size_t Length of the span
  inline size_t size() const VB_NOEXCEPT {
    return size_;
  }

  ///
  /// @brief Accesses an element at a given index. Does not perform bounds checks.
  ///
  /// @param index Index of the element
  /// @return T& Reference to that element
  T &operator[](size_t const index) VB_NOEXCEPT {
    return *(data_ + index);
  }

  ///
  /// @brief Accesses an element at a given index. Does not perform bounds checks.
  ///
  /// @param index Index of the element
  /// @return T& Reference to that element
  T const &operator[](size_t const index) const VB_NOEXCEPT {
    return *(data_ + index);
  }

  ///
  /// @brief Resets the memory object with a new pointer and length
  ///
  /// @param data New base pointer
  /// @param size New length
  inline void reset(T *const data, size_t const size) VB_NOEXCEPT {
    data_ = data;
    size_ = size;
  }

  /// @brief first iterator
  inline T *begin() const VB_NOEXCEPT {
    return data_;
  }

  /// @brief last iterator
  inline T *end() const VB_NOEXCEPT {
    return data_ + size_;
  }

  /// @brief contains element
  /// @param element element to check contains
  /// @return true if span contains element
  template <class E> inline bool contains(E const &element) const VB_NOEXCEPT {
    for (size_t i{0U}; i < size_; i++) {
      if (data_[i] == element) {
        return true;
      }
    }
    return false;
  }

  /// @brief obtains a span that is a view over the @param count elements of this span starting at offset @param offset
  inline Span<T> subspan(size_t const offset, size_t const count = std::numeric_limits<size_t>::max()) const VB_NOEXCEPT {
    return Span<T>{data_ + offset, std::min(count, size_ - offset)};
  }

private:
  ///
  /// @brief Pointer to the start of the memory region
  ///
  T *data_;

  ///
  /// @brief Length of the referenced memory
  ///
  size_t size_;
};

} // namespace vb

#endif

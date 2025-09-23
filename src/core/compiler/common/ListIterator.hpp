///
/// @file ListIterator.hpp
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
#ifndef LIST_ITERATOR_HPP
#define LIST_ITERATOR_HPP

#include "src/core/common/util.hpp"

namespace vb {

/// @brief A double-linked list node
template <typename T> class List_node final {
  static_assert(std::is_same<typename std::remove_cv<T>::type, T>::value, "List_node must have a non-const, non-volatile value_type");

public:
  T value;         ///< value
  List_node *prev; ///< prev
  List_node *next; ///< next

  /// @brief constructor
  explicit List_node(const T &val = T(), List_node *p = nullptr, List_node *n = nullptr) : value(val), prev(p), next(n) {
  }
};

/// @brief iterator
template <typename T> class List_iterator final {
  static_assert(std::is_same<typename std::remove_cv<T>::type, T>::value, "List_node must have a non-const, non-volatile value_type");

public:
  using pointer = T *;    ///< pointer
  using reference = T &;  ///< reference
  List_node<T> *current_; ///< points to the node

  /// @brief constructor
  constexpr List_iterator() VB_NOEXCEPT : List_iterator(nullptr) {
  }

  /// @brief constructor
  explicit constexpr List_iterator(List_node<T> *const node) VB_NOEXCEPT : current_(node) {
  }

  /// @brief check if is empty
  inline bool isEmpty() const VB_NOEXCEPT {
    return current_ == nullptr;
  }

  /// @brief get valid pointer
  pointer unwrap() const VB_NOEXCEPT {
    // coverity[autosar_cpp14_m9_3_1_violation]
    return &(current_->value);
  }

  /// @brief get raw pointer
  pointer raw() const VB_NOEXCEPT {
    return isEmpty() ? nullptr : &(current_->value);
  }

  /// @brief deref
  reference operator*() const VB_NOEXCEPT {
    // coverity[autosar_cpp14_m9_3_1_violation]
    return current_->value;
  }

  /// @brief deref
  pointer operator->() const VB_NOEXCEPT {
    // coverity[autosar_cpp14_m9_3_1_violation]
    return &(current_->value);
  }

  /// @brief next iterator
  List_iterator next() const VB_NOEXCEPT {
    return List_iterator(current_->next);
  }

  /// @brief prev iterator
  List_iterator prev() const VB_NOEXCEPT {
    return List_iterator(current_->prev);
  }

  /// @brief inc
  List_iterator &operator++() VB_NOEXCEPT {
    *this = next();
    return *this;
  }

  /// @brief inc
  List_iterator operator++(int) VB_NOEXCEPT {
    List_iterator const tmp{*this};
    *this = next();
    return tmp;
  }

  /// @brief dec
  List_iterator &operator--() VB_NOEXCEPT {
    *this = prev();
    return *this;
  }

  /// @brief dec
  List_iterator operator--(int) VB_NOEXCEPT {
    List_iterator const tmp{*this};
    *this = prev();
    return tmp;
  }

  /// @brief equal
  friend bool operator==(List_iterator const lhs, List_iterator const rhs) VB_NOEXCEPT {
    return lhs.current_ == rhs.current_;
  }

  /// @brief not equal
  friend bool operator!=(List_iterator const lhs, List_iterator const rhs) VB_NOEXCEPT {
    return !(lhs == rhs);
  }
};

/// @brief A const iterator
template <typename T> class List_const_iterator final {
  static_assert(std::is_same<typename std::remove_cv<T>::type, T>::value, "List_node must have a non-const, non-volatile value_type");

public:
  using pointer = const T *;    ///< pointer
  using reference = const T &;  ///< reference
  const List_node<T> *current_; ///< points to the node

  /// @brief constructor
  constexpr List_const_iterator() VB_NOEXCEPT : List_const_iterator(nullptr) {
  }

  /// @brief constructor
  explicit constexpr List_const_iterator(List_node<T> const *const node) VB_NOEXCEPT : current_(node) {
  }

  /// @brief constructor
  explicit constexpr List_const_iterator(const List_iterator<T> &it) VB_NOEXCEPT : current_(it.current_) {
  }

  /// @brief check if is empty
  inline bool isEmpty() const VB_NOEXCEPT {
    return current_ != nullptr;
  }

  /// @brief get valid pointer
  pointer unwrap() const VB_NOEXCEPT {
    assert(isEmpty() && "Invalid iterator");
    return &(current_->value);
  }

  /// @brief get raw pointer
  pointer raw() const VB_NOEXCEPT {
    return &(current_->value);
  }

  /// @brief deref
  reference operator*() const VB_NOEXCEPT {
    return current_->value;
  }

  /// @brief deref
  pointer operator->() const VB_NOEXCEPT {
    return current_->value;
  }

  /// @brief next iterator
  List_const_iterator next() const VB_NOEXCEPT {
    return List_const_iterator(current_->next);
  }

  /// @brief prev iterator
  List_const_iterator prev() const VB_NOEXCEPT {
    return List_const_iterator(current_->prev);
  }

  /// @brief inc
  List_const_iterator &operator++() VB_NOEXCEPT {
    *this = next();
    return *this;
  }

  /// @brief inc
  List_const_iterator operator++(int) VB_NOEXCEPT {
    List_const_iterator const tmp{*this};
    *this = next();
    return tmp;
  }

  /// @brief dec
  List_const_iterator &operator--() VB_NOEXCEPT {
    *this = prev();
    return *this;
  }

  /// @brief dec
  List_const_iterator operator--(int) VB_NOEXCEPT {
    List_const_iterator const tmp{*this};
    *this = prev();
    return tmp;
  }

  /// @brief equal
  friend bool operator==(List_const_iterator const lhs, List_const_iterator const rhs) VB_NOEXCEPT {
    return lhs.current_ == rhs.current_;
  }

  /// @brief not equal
  friend bool operator!=(List_const_iterator const lhs, List_const_iterator const rhs) VB_NOEXCEPT {
    return !(lhs == rhs);
  }
};
} // namespace vb

#endif // LIST_ITERATOR_HPP

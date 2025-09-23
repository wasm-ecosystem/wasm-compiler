///
/// @file VbExceptions.hpp
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
#ifndef CUSTOMEXCEPTIONS_HPP
#define CUSTOMEXCEPTIONS_HPP

#include <cstdint>
#include <exception>

#include "src/config.hpp"

namespace vb {
///
/// @brief Runtime error without dynamic memory allocation
///
class RuntimeError : public std::exception {
public:
  ///
  /// @brief Definition of error codes
  ///
  enum class Code : uint16_t {
    Could_not_extend_memory,
    Maximum_stack_trace_record_count_is_50,
    Reached_maximum_stack_frame_size,
    Cannot_export_builtin_function,
    Cannot_indirectly_call_builtin_functions,
    Conditional_branches_or_adr_can_only_target_offsets_in_the_range___1MB,
    Small_branches_can_only_target_unsigned_offsets_in_the_range___32B,
    Branches_can_only_target_offsets_in_the_range___128MB,
    Conditional_branches_or_lea_can_only_target_offsets_in_the_range___32kB,
    Branches_can_only_target_offsets_in_the_range___16MB,
    Maximum_offset_reached,
    BrHANDLE_ERRORanches_can_only_maximally_target_32_bit_signed_offsets,
    Wrong_type,
    Maximum_number_of_bytes_written,
    Bytecode_out_of_range,
    Malformed_LEB128_integer__Out_of_bounds_,
    Malformed_LEB128_signed_integer__Wrong_padding_,
    Malformed_LEB128_unsigned_integer__Wrong_padding_,
    Malformed_UTF_8_sequence,
    Function_type_out_of_bounds,
    Invalid_branch_depth,
    Wrong_Wasm_magic_number,
    Wasm_Version_not_supported,
    Too_many_types,
    Malformed_section_1__wrong_type,
    Too_many_params,
    Too_many_results,
    Invalid_function_parameter_type,
    Invalid_function_return_type,
    Module_name_too_long,
    Import_name_too_long,
    Function_type_index_out_of_bounds,
    Imported_symbol_could_not_be_found,
    Imported_table_not_supported,
    Imported_memory_not_supported,
    Imported_global_not_supported,
    Unknown_import_type,
    Too_many_imported_functions,
    Maximum_number_of_functions_exceeded,
    Only_table_type__funcref__allowed,
    Unknown_size_limit_flag,
    Table_initial_size_too_long,
    Maximum_table_size_smaller_than_initial_table_size,
    Table_Maximum_Size_too_long,
    Only_one_memory_instance_allowed,
    Maximum_memory_size_smaller_than_initial_memory_size,
    Memory_size_must_be_at_most_65536_pages__4GiB_,
    Too_many_globals,
    Invalid_global_type,
    Unknown_mutability_flag,
    Malformed_global_initialization_expression,
    Imported_globals_not_supported,
    Export_name_too_long,
    Unknown_export_type,
    Function_out_of_range,
    Global_out_of_range,
    Memory_out_of_range,
    Table_out_of_range,
    Duplicate_export_symbol,
    Start_function_index_out_of_range,
    Start_function_must_be_nullary,
    Table_index_out_of_bounds,
    Constant_expression_offset_has_to_be_of_type_i32,
    Malformed_constant_expression_offset,
    Table_element_index_out_of_range__initial_table_size_,
    Function_index_out_of_range,
    Function_and_code_section__mismatch_of_number_of_definitions,
    Too_many_direct_locals,
    Invalid_local_type_in_function,
    Type_mismatch_for_if_true_and_false_branches,
    Too_many_branch_targets_in_br_table,
    br_table_block_return_type_mismatch,
    Table_not_found,
    Local_out_of_range,
    Cannot_set_immutable_global,
    Undefined_memory_referenced,
    Alignment_out_of_range,
    memory_size_reserved_value_must_be_a_zero_byte,
    memory_grow_reserved_value_must_be_a_zero_byte,
    Unknown_instruction,
    Function_was_not_terminated_properly,
    Function_size_mismatch,
    Memory_index_out_of_bounds,
    Data_count_and_data_section_have_inconsistent_lengths,
    Data_segment_out_of_initial_bounds,
    Invalid_data_segment_kind,
    Subsection_size_mismatch,
    Missing_function_bodies,
    Name_section_must_not_appear_before_data_section,
    Empty_input,
    Section_of_size_0,
    Section_size_extends_past_module_size,
    Duplicate_section_or_sections_in_wrong_order,
    Multiple_name_sections_encountered,
    Invalid_section_type,
    Section_size_mismatch,
    Runtime_is_disabled,
    Module_not_initialized__Call_start_function_first_,
    Cannot_initialize_runtime_when_dummy_imports_are_used__This_mode_should_only_be_used_to_benchmark_the_compiler_,
    Base_of_job_memory_not_8_byte_aligned,
    Start_function_has_already_been_called,
    Dynamic_import_not_resolved,
    Could_not_extend_linear_memory,
    Stack_fence_too_high,
    Cannot_keep_STACKSIZE_LEFT_BEFORE_NATIVE_CALL_free_before_native_call__Stack_fence_too_high_,
    Memory_reallocation_failed,
    Linear_memory_address_out_of_bounds,
    Module_memory_not_16_byte_aligned,
    Module_memory_not_8_byte_aligned,
    Function_not_found,
    Global_not_found,
    Global_type_mismatch,
    Global_is_immutable_and_cannot_be_written,
    Function_signature_mismatch,
    Function_signature_mismatch__signature_size_mismatch,
    Function_signature_mismatch__wrong_parameter_type,
    Function_signature_mismatch__wrong_return_type,
    Function_signature_mismatch__invalid_signature_type,
    Limit_too_low__memory_already_in_use,
    can_t_open__proc_self_as,
    AddVectoredExceptionHandler_failed,
    SetThreadStackGuarantee_failed,
    Syscall_failed,

    Bulk_memory_operations_feature_not_implemented,
    Reference_type_feature_not_implemented,
    Passive_mode_data_segments_not_implemented,
    Non_trapping_float_to_int_conversions_not_implemented,
    Simd_feature_not_implemented,

    Not_implemented,

    Binary_module_version_not_supported,

    // Validation stack specific error codes
    validateAndDrop__Stack_frame_underflow,
    Validation_failed,
  };

  ///
  /// @brief type covert to const char*
  /// @return error message
  ///
  explicit operator const char *() const VB_NOEXCEPT;
  ///
  /// @brief construct a RuntimeError
  /// @param code the internal code
  ///
  // NOLINTNEXTLINE(readability-redundant-member-init)
  inline explicit RuntimeError(Code const code) VB_NOEXCEPT : std::exception(), code_(code) {
  }

  ///
  /// @brief Default copy constructor
  ///
  // coverity[autosar_cpp14_a12_8_6_violation]
  RuntimeError(const RuntimeError &) = default;
  ///
  /// @brief Default move constructor
  ///
  // coverity[autosar_cpp14_a12_8_6_violation]
  RuntimeError(RuntimeError &&) = default;
  ///
  /// @brief Default copy operator
  ///
  // coverity[autosar_cpp14_a12_8_6_violation]
  RuntimeError &operator=(const RuntimeError &) & = default;
  ///
  /// @brief Default move operator
  ///
  // coverity[autosar_cpp14_a12_8_6_violation]
  RuntimeError &operator=(RuntimeError &&) &VB_NOEXCEPT = default;
  ///
  /// @brief Default destructor
  ///
  ~RuntimeError() VB_NOEXCEPT override = default;

  ///
  /// @brief get error message of current error code
  /// @return error message
  ///
  inline const char *what() const noexcept override {
    return static_cast<const char *>(*this);
  }

private:
  Code code_; ///< The internal error code
};

/// @brief All errors in WARP
using ErrorCode = RuntimeError::Code;

///
/// @brief Exception to be thrown if a validation error during parsing of the WebAssembly bytecode is encountered
///
class ValidationException final : public RuntimeError {
public:
  using RuntimeError::RuntimeError;

  ///
  /// @brief Default copy constructor
  ///
  ValidationException(const ValidationException &) = default;
  ///
  /// @brief Default move constructor
  ///
  ValidationException(ValidationException &&) = default;
  ///
  /// @brief Default copy operator
  ///
  ValidationException &operator=(const ValidationException &) & = default;
  ///
  /// @brief Default move operator
  ///
  ValidationException &operator=(ValidationException &&) &VB_NOEXCEPT = default;
  ///
  /// @brief Default destructor
  ///
  ~ValidationException() VB_NOEXCEPT final = default;
};

///
/// @brief Exception to be thrown if an import of the WebAssembly module cannot be found/resolved
///
class LinkingException final : public RuntimeError {
public:
  using RuntimeError::RuntimeError;

  ///
  /// @brief Default copy constructor
  ///
  LinkingException(const LinkingException &) = default;
  ///
  /// @brief Default move constructor
  ///
  LinkingException(LinkingException &&) = default;
  ///
  /// @brief Default copy operator
  ///
  LinkingException &operator=(const LinkingException &) & = default;
  ///
  /// @brief Default move operator
  ///
  LinkingException &operator=(LinkingException &&) &VB_NOEXCEPT = default;
  ///
  /// @brief Default destructor
  ///
  ~LinkingException() VB_NOEXCEPT final = default;
};

///
/// @brief Exception to be thrown if a feature is not supported by this specific implementation, but would generally be
/// supported by the WebAssembly specification
///
class FeatureNotSupportedException final : public RuntimeError {
public:
  using RuntimeError::RuntimeError;

  ///
  /// @brief Default copy constructor
  ///
  FeatureNotSupportedException(const FeatureNotSupportedException &) = default;
  ///
  /// @brief Default move constructor
  ///
  FeatureNotSupportedException(FeatureNotSupportedException &&) = default;
  ///
  /// @brief Default copy operator
  ///
  FeatureNotSupportedException &operator=(const FeatureNotSupportedException &) & = default;
  ///
  /// @brief Default move operator
  ///
  FeatureNotSupportedException &operator=(FeatureNotSupportedException &&) &VB_NOEXCEPT = default;
  ///
  /// @brief Default destructor
  ///
  ~FeatureNotSupportedException() VB_NOEXCEPT final = default;
};

///
/// @brief Exception to be thrown if a feature is supported, but the extent (e.g. number of local variables) exceed the
/// implementation limit
///
class ImplementationLimitationException final : public RuntimeError {
public:
  using RuntimeError::RuntimeError;

  ///
  /// @brief Default copy constructor
  ///
  ImplementationLimitationException(const ImplementationLimitationException &) = default;
  ///
  /// @brief Default move constructor
  ///
  ImplementationLimitationException(ImplementationLimitationException &&) = default;
  ///
  /// @brief Default copy operator
  ///
  ImplementationLimitationException &operator=(const ImplementationLimitationException &) & = default;
  ///
  /// @brief Default move operator
  ///
  ImplementationLimitationException &operator=(ImplementationLimitationException &&) &VB_NOEXCEPT = default;
  ///
  /// @brief Default destructor
  ///
  ~ImplementationLimitationException() VB_NOEXCEPT final = default;
};

} // namespace vb

#endif

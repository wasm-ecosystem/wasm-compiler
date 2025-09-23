///
/// @file VbExceptions.cpp
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
#include "VbExceptions.hpp"

#include "src/config.hpp"

namespace vb {
RuntimeError::operator const char *() const VB_NOEXCEPT {
  switch (code_) {
  case (Code::Could_not_extend_memory): {
    return "Could not extend memory";
  }
  case (Code::Maximum_stack_trace_record_count_is_50): {
    return "Maximum stack trace record count is 50";
  }
  case (Code::Reached_maximum_stack_frame_size): {
    return "Reached maximum stack frame size";
  }
  case (Code::Cannot_export_builtin_function): {
    return "Cannot export builtin function";
  }
  case (Code::Cannot_indirectly_call_builtin_functions): {
    return "Cannot indirectly call builtin functions";
  }
  case (Code::Conditional_branches_or_adr_can_only_target_offsets_in_the_range___1MB): {
    return "Conditional branches or ADR can only target offsets in the range +-1MB";
  }
  case (Code::Small_branches_can_only_target_unsigned_offsets_in_the_range___32B): {
    return "Small branches (16-bit) can only target unsigned offsets in the range +32B";
  }
  case (Code::Branches_can_only_target_offsets_in_the_range___128MB): {
    return "Branches can only target offsets in the range +-128MB";
  }
  case (Code::Conditional_branches_or_lea_can_only_target_offsets_in_the_range___32kB): {
    return "Conditional branches or LEA can only target offsets in the range +-32kB";
  }
  case (Code::Branches_can_only_target_offsets_in_the_range___16MB): {
    return "Branches can only target offsets in the range +-16MB";
  }
  case (Code::Maximum_offset_reached): {
    return "Maximum offset reached";
  }
  case (Code::BrHANDLE_ERRORanches_can_only_maximally_target_32_bit_signed_offsets): {
    return "Branches can only maximally target 32-bit signed offsets";
  }
  case (Code::Wrong_type): {
    return "Wrong type";
  }
  case (Code::validateAndDrop__Stack_frame_underflow): {
    return "validateAndDrop: Stack frame underflow";
  }
  case (Code::Maximum_number_of_bytes_written): {
    return "Maximum number of bytes written";
  }
  case (Code::Bytecode_out_of_range): {
    return "Bytecode out of range";
  }
  case (Code::Malformed_LEB128_integer__Out_of_bounds_): {
    return "Malformed LEB128 integer Out of bounds";
  }
  case (Code::Malformed_LEB128_signed_integer__Wrong_padding_): {
    return "Malformed LEB128 signed integer Wrong padding";
  }
  case (Code::Malformed_LEB128_unsigned_integer__Wrong_padding_): {
    return "Malformed LEB128 unsigned integer Wrong padding";
  }
  case (Code::Malformed_UTF_8_sequence): {
    return "Malformed UTF-8 sequence";
  }
  case (Code::Function_type_out_of_bounds): {
    return "Function type out of bounds";
  }
  case (Code::Invalid_branch_depth): {
    return "Invalid branch depth";
  }
  case (Code::Wrong_Wasm_magic_number): {
    return "Wrong Wasm magic number";
  }
  case (Code::Wasm_Version_not_supported): {
    return "Wasm Version not supported";
  }
  case (Code::Too_many_types): {
    return "Too many types";
  }
  case (Code::Malformed_section_1__wrong_type): {
    return "Malformed section 1, wrong type";
  }
  case (Code::Too_many_params): {
    return "Too many params";
  }
  case (Code::Too_many_results): {
    return "Too many results";
  }
  case (Code::Invalid_function_parameter_type): {
    return "Invalid function parameter type";
  }
  case (Code::Invalid_function_return_type): {
    return "Invalid function return type";
  }
  case (Code::Module_name_too_long): {
    return "Module name too long";
  }
  case (Code::Import_name_too_long): {
    return "Import name too long";
  }
  case (Code::Function_type_index_out_of_bounds): {
    return "Function type index out of bounds";
  }
  case (Code::Imported_symbol_could_not_be_found): {
    return "Imported symbol could not be found";
  }
  case (Code::Imported_table_not_supported): {
    return "Imported table not supported";
  }
  case (Code::Imported_memory_not_supported): {
    return "Imported memory not supported";
  }
  case (Code::Imported_global_not_supported): {
    return "Imported global not supported";
  }
  case (Code::Unknown_import_type): {
    return "Unknown import type";
  }
  case (Code::Too_many_imported_functions): {
    return "Too many imported functions";
  }
  case (Code::Maximum_number_of_functions_exceeded): {
    return "Maximum number of functions exceeded";
  }
  case (Code::Only_table_type__funcref__allowed): {
    return "Only table type 'funcref' allowed";
  }
  case (Code::Unknown_size_limit_flag): {
    return "Unknown size limit flag";
  }
  case (Code::Table_initial_size_too_long): {
    return "Table initial size too long";
  }
  case (Code::Maximum_table_size_smaller_than_initial_table_size): {
    return "Maximum table size smaller than initial table size";
  }
  case (Code::Table_Maximum_Size_too_long): {
    return "Table Maximum Size too long";
  }
  case (Code::Only_one_memory_instance_allowed): {
    return "Only one memory instance allowed";
  }
  case (Code::Maximum_memory_size_smaller_than_initial_memory_size): {
    return "Maximum memory size smaller than initial memory size";
  }
  case (Code::Memory_size_must_be_at_most_65536_pages__4GiB_): {
    return "Memory size must be at most 65536 pages 4GiB";
  }
  case (Code::Too_many_globals): {
    return "Too many globals";
  }
  case (Code::Invalid_global_type): {
    return "Invalid global type";
  }
  case (Code::Unknown_mutability_flag): {
    return "Unknown mutability flag";
  }
  case (Code::Malformed_global_initialization_expression): {
    return "Malformed global initialization expression";
  }
  case (Code::Imported_globals_not_supported): {
    return "Imported globals not supported";
  }
  case (Code::Export_name_too_long): {
    return "Export name too long";
  }
  case (Code::Unknown_export_type): {
    return "Unknown export type";
  }
  case (Code::Function_out_of_range): {
    return "Function out of range";
  }
  case (Code::Global_out_of_range): {
    return "Global out of range";
  }
  case (Code::Memory_out_of_range): {
    return "Memory out of range";
  }
  case (Code::Table_out_of_range): {
    return "Table out of range";
  }
  case (Code::Duplicate_export_symbol): {
    return "Duplicate export symbol";
  }
  case (Code::Start_function_index_out_of_range): {
    return "Start function index out of range";
  }
  case (Code::Start_function_must_be_nullary): {
    return "Start function must be nullary";
  }
  case (Code::Table_index_out_of_bounds): {
    return "Table index out of bounds";
  }
  case (Code::Constant_expression_offset_has_to_be_of_type_i32): {
    return "Constant expression offset has to be of type i32";
  }
  case (Code::Malformed_constant_expression_offset): {
    return "Malformed constant expression offset";
  }
  case (Code::Table_element_index_out_of_range__initial_table_size_): {
    return "Table element index out of range initial table size";
  }
  case (Code::Function_index_out_of_range): {
    return "Function index out of range";
  }
  case (Code::Function_and_code_section__mismatch_of_number_of_definitions): {
    return "Function and code section: mismatch of number of definitions";
  }
  case (Code::Too_many_direct_locals): {
    return "Too many direct locals";
  }
  case (Code::Invalid_local_type_in_function): {
    return "Invalid local type in function";
  }
  case (Code::Type_mismatch_for_if_true_and_false_branches): {
    return "Type mismatch for if true and false branches";
  }
  case (Code::Too_many_branch_targets_in_br_table): {
    return "Too many branch targets in br_table";
  }
  case (Code::br_table_block_return_type_mismatch): {
    return "br_table block return type mismatch";
  }
  case (Code::Table_not_found): {
    return "Table not found";
  }
  case (Code::Local_out_of_range): {
    return "Local out of range";
  }
  case (Code::Cannot_set_immutable_global): {
    return "Cannot set immutable global";
  }
  case (Code::Undefined_memory_referenced): {
    return "Undefined memory referenced";
  }
  case (Code::Alignment_out_of_range): {
    return "Alignment out of range";
  }
  case (Code::memory_size_reserved_value_must_be_a_zero_byte): {
    return "memory.size reserved value must be a zero byte";
  }
  case (Code::memory_grow_reserved_value_must_be_a_zero_byte): {
    return "memory.grow reserved value must be a zero byte";
  }
  case (Code::Unknown_instruction): {
    return "Unknown instruction";
  }
  case (Code::Function_was_not_terminated_properly): {
    return "Function was not terminated properly";
  }
  case (Code::Function_size_mismatch): {
    return "Function size mismatch";
  }
  case (Code::Memory_index_out_of_bounds): {
    return "Memory index out of bounds";
  }
  case (Code::Data_count_and_data_section_have_inconsistent_lengths): {
    return "Data count and data section have inconsistent lengths";
  }
  case (Code::Data_segment_out_of_initial_bounds): {
    return "Data segment out of initial bounds";
  }
  case (Code::Invalid_data_segment_kind): {
    return "Invalid data segment kind";
  }
  case (Code::Subsection_size_mismatch): {
    return "Subsection size mismatch";
  }
  case (Code::Missing_function_bodies): {
    return "Missing function bodies";
  }
  case (Code::Name_section_must_not_appear_before_data_section): {
    return "Name section must not appear before data section";
  }
  case (Code::Empty_input): {
    return "Empty input";
  }
  case (Code::Section_of_size_0): {
    return "Section of size 0";
  }
  case (Code::Section_size_extends_past_module_size): {
    return "Section size extends past module size";
  }
  case (Code::Duplicate_section_or_sections_in_wrong_order): {
    return "Duplicate section or sections in wrong order";
  }
  case (Code::Multiple_name_sections_encountered): {
    return "Multiple name sections encountered";
  }
  case (Code::Invalid_section_type): {
    return "Invalid section type";
  }
  case (Code::Section_size_mismatch): {
    return "Section size mismatch";
  }
  case (Code::Runtime_is_disabled): {
    return "Runtime is disabled";
  }
  case (Code::Module_not_initialized__Call_start_function_first_): {
    return "Module not initialized. Call start function first.";
  }
  case (Code::Cannot_initialize_runtime_when_dummy_imports_are_used__This_mode_should_only_be_used_to_benchmark_the_compiler_): {
    return "Cannot initialize runtime when dummy imports are used. This mode should only be used to benchmark the "
           "compiler.";
  }
  case (Code::Base_of_job_memory_not_8_byte_aligned): {
    return "Base of job memory not 8-byte aligned";
  }
  case (Code::Start_function_has_already_been_called): {
    return "Start function has already been called";
  }
  case (Code::Dynamic_import_not_resolved): {
    return "Dynamic import not resolved";
  }
  case (Code::Could_not_extend_linear_memory): {
    return "Could not extend linear memory";
  }
  case (Code::Stack_fence_too_high): {
    return "Stack fence too high";
  }
  case (Code::Cannot_keep_STACKSIZE_LEFT_BEFORE_NATIVE_CALL_free_before_native_call__Stack_fence_too_high_): {
    return "Cannot keep STACKSIZE_LEFT_BEFORE_NATIVE_CALL free before native call. Stack fence too high.";
  }
  case (Code::Memory_reallocation_failed): {
    return "Memory reallocation failed";
  }
  case (Code::Linear_memory_address_out_of_bounds): {
    return "Linear memory address out of bounds";
  }
  case (Code::Module_memory_not_16_byte_aligned): {
    return "Module memory not 16-byte aligned";
  }
  case (Code::Module_memory_not_8_byte_aligned): {
    return "Module memory not 8-byte aligned";
  }
  case (Code::Function_not_found): {
    return "Function not found";
  }
  case (Code::Global_not_found): {
    return "Global not found";
  }
  case (Code::Global_type_mismatch): {
    return "Global type mismatch";
  }
  case (Code::Global_is_immutable_and_cannot_be_written): {
    return "Global is immutable and cannot be written";
  }
  case (Code::Function_signature_mismatch): {
    return "Function signature mismatch";
  }
  case (Code::Function_signature_mismatch__signature_size_mismatch): {
    return "Function signature mismatch, signature size mismatch";
  }
  case (Code::Function_signature_mismatch__wrong_parameter_type): {
    return "Function signature mismatch, wrong parameter type";
  }
  case (Code::Function_signature_mismatch__wrong_return_type): {
    return "Function signature mismatch, wrong return type";
  }
  case (Code::Function_signature_mismatch__invalid_signature_type): {
    return "Function signature mismatch, invalid signature type";
  }
  case (Code::Limit_too_low__memory_already_in_use): {
    return "Limit too low, memory already in use";
  }
  case (Code::can_t_open__proc_self_as): {
    return "can't open /proc/self/as";
  }
  case (Code::AddVectoredExceptionHandler_failed): {
    return "AddVectoredExceptionHandler failed";
  }
  case (Code::SetThreadStackGuarantee_failed): {
    return "SetThreadStackGuarantee failed";
  }
  case (Code::Syscall_failed): {
    return "System call failed";
  }

  case (Code::Bulk_memory_operations_feature_not_implemented): {
    return "Bulk Memory Operations feature not implemented";
  }
  case (Code::Reference_type_feature_not_implemented): {
    return "Reference Type feature not implemented";
  }
  case (Code::Passive_mode_data_segments_not_implemented): {
    return "Passive mode data segments not implemented";
  }
  case (Code::Non_trapping_float_to_int_conversions_not_implemented): {
    return "Non-trapping Float-to-int Conversions feature not implemented";
  }
  case (Code::Simd_feature_not_implemented): {
    return "SIMD not implemented";
  }
  case (Code::Binary_module_version_not_supported): {
    return "Binary module version not supported";
  }
  case (Code::Not_implemented): {
    return "Not implemented";
  }
  case (Code::Validation_failed): {
    return "Validation failed";
  }

  default:
    return "unknown error";
  }
}
} // namespace vb

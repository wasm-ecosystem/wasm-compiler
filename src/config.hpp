///
/// @file config.hpp
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
#ifndef CONFIG_HPP
#define CONFIG_HPP

// ----------------------------------------------------------------------------
// Config
// ----------------------------------------------------------------------------

// ----------------------------------------------------------------------------
// AUTOCONFIG
// Better not change anything below unless you know what you are doing
// ----------------------------------------------------------------------------

// CPU Architectures
#ifndef ISA_X86_64
#define ISA_X86_64 1
#endif

#ifndef ISA_AARCH64
#define ISA_AARCH64 2
#endif

#ifndef ISA_TRICORE
#define ISA_TRICORE 3
#endif

#if (defined __x86_64__) || (defined _M_X64)
#ifndef CXX_TARGET
#define CXX_TARGET ISA_X86_64
#endif
#elif (defined __arm64__) || (defined __aarch64__) || (defined _M_ARM64)
#ifndef CXX_TARGET
#define CXX_TARGET ISA_AARCH64
#endif
#elif (defined __tricore__) || (defined __CPTC__)
#ifndef CXX_TARGET
#define CXX_TARGET ISA_TRICORE
#endif
#else
static_assert(false, "unsupported target CPU");
#endif

// JIT Target
#if !(defined JIT_TARGET_X86_64) && !(defined JIT_TARGET_AARCH64) && !(defined JIT_TARGET_TRICORE)
#if CXX_TARGET == ISA_X86_64
#define JIT_TARGET_X86_64
#elif CXX_TARGET == ISA_AARCH64
#define JIT_TARGET_AARCH64
#elif CXX_TARGET == ISA_TRICORE
#define JIT_TARGET_TRICORE
#else
static_assert(false, "JIT_TARGET is not set");
#endif
#endif

#ifndef JIT_TARGET

#if defined(JIT_TARGET_X86_64)
#define JIT_TARGET ISA_X86_64
#elif defined(JIT_TARGET_AARCH64)
#define JIT_TARGET ISA_AARCH64
#elif defined(JIT_TARGET_TRICORE)
#define JIT_TARGET ISA_TRICORE
#endif

#endif // JIT_TARGET

#ifndef TARGET_RTOS
#ifdef __QNX__
#define TARGET_RTOS 1
#else
#define TARGET_RTOS 0
#endif
#endif

// features
#ifndef INTERRUPTION_REQUEST
#define INTERRUPTION_REQUEST 1
#endif

#ifndef EAGER_ALLOCATION
#define EAGER_ALLOCATION 0
#endif

#ifndef BUILTIN_FUNCTIONS
#define BUILTIN_FUNCTIONS 1
#endif

#ifndef ACTIVE_DIV_CHECK
#if (defined JIT_TARGET_TRICORE) || (JIT_TARGET == ISA_AARCH64) || (TARGET_RTOS == 1) || (defined __MINGW32__)
#define ACTIVE_DIV_CHECK 1
#else
#define ACTIVE_DIV_CHECK 0
#endif
#endif

#ifndef ACTIVE_STACK_OVERFLOW_CHECK
#if (defined JIT_TARGET_TRICORE) || (TARGET_RTOS == 1) || (defined __MINGW32__) // Don't mix use of poxis and win32 signals on mingw
#define ACTIVE_STACK_OVERFLOW_CHECK 1
#else
#define ACTIVE_STACK_OVERFLOW_CHECK 0
#endif
#endif

#ifndef LINEAR_MEMORY_BOUNDS_CHECKS
#if (defined JIT_TARGET_TRICORE) || (TARGET_RTOS == 1) // RTOS usually have restrict usage of poxis signals, use active check by default
#define LINEAR_MEMORY_BOUNDS_CHECKS 1
#else
#define LINEAR_MEMORY_BOUNDS_CHECKS 0
#endif
#endif

#ifndef ENABLE_EXTENSIONS
#define ENABLE_EXTENSIONS 0
#endif

// #define VALGRIND // Enable Valgrind instrumentation, NOTE: Needs valgrind headers

// NOTE: Should only be set for passive stack overflow protection
#ifndef MAX_WASM_STACKSIZE_BEFORE_NATIVE_CALL
// Set to zero to make stack "unlimited" before native call
#define MAX_WASM_STACKSIZE_BEFORE_NATIVE_CALL 0
#endif

// NOTE: Should only be set for active stack overflow protection
#ifndef STACKSIZE_LEFT_BEFORE_NATIVE_CALL
// Set to zero to not reserve stack space before calling C++ functions from Wasm
#define STACKSIZE_LEFT_BEFORE_NATIVE_CALL 0
#endif

#ifdef JIT_TARGET_TRICORE
#ifndef NO_PASSIVE_PROTECTION_WARNING
#define NO_PASSIVE_PROTECTION_WARNING
#endif

#if (defined __CPTC__) && (defined __CORE_TC18__)

#ifndef TC_USE_HARD_F32_TO_I32_CONVERSIONS
#define TC_USE_HARD_F32_TO_I32_CONVERSIONS 1
#endif

#endif

#ifndef TC_USE_HARD_F32_ARITHMETICS
#define TC_USE_HARD_F32_ARITHMETICS 0
#endif

#if TC_USE_HARD_F32_ARITHMETICS
#warning "TriCore F32 arithmetics are not IEEE754 conforming"
#endif

#ifndef TC_LINK_AUX_FNCS_DYNAMICALLY
#if (CXX_TARGET != ISA_TRICORE)
// On 64-bit development machines, due to the loss of precision in the conversion from uintptr_t to uint32_t, for
// reproducibility reasons, every function should be linked dynamically.
#define TC_LINK_AUX_FNCS_DYNAMICALLY 1
#else
#define TC_LINK_AUX_FNCS_DYNAMICALLY 0
#endif
#endif

#ifndef TC_USE_HARD_F32_TO_I32_CONVERSIONS
#define TC_USE_HARD_F32_TO_I32_CONVERSIONS 0
#endif

#ifndef TC_USE_DIV
#define TC_USE_DIV 1
#endif

#endif

#if (defined __linux__) || (defined __APPLE__) || ((defined(__unix__) || defined(__unix)) && !(defined __MINGW32__))
#ifndef VB_POSIX
#define VB_POSIX
#endif
#endif

#if (defined _WIN32) || (defined __MINGW32__)
#ifndef VB_WIN32
#define VB_WIN32
#endif
#endif

#if (defined VB_POSIX) || (defined VB_WIN32)
#ifndef VB_WIN32_OR_POSIX
#define VB_WIN32_OR_POSIX
#endif
#endif

// ----------------------------------------------------------------------------
// Stack Overflow Protection
#if (((defined _WIN32) || (defined __linux__) || (defined __APPLE__)) && !(defined __MINGW32__))
#define TARGET_SUPPORTS_PASSIVE_STACK_OVERFLOW_PROTECTION 1
#else
#define TARGET_SUPPORTS_PASSIVE_STACK_OVERFLOW_PROTECTION 0
#endif

#if TARGET_SUPPORTS_PASSIVE_STACK_OVERFLOW_PROTECTION && ACTIVE_STACK_OVERFLOW_CHECK && (TARGET_RTOS == 0)
#ifndef NO_PASSIVE_PROTECTION_WARNING
// coverity[autosar_cpp14_a16_7_1_violation]
#pragma message("Your target seems to support passive stack overflow protection. Enabling active stack overflow check will degrade performance.")
#endif
#endif

#if !TARGET_SUPPORTS_PASSIVE_STACK_OVERFLOW_PROTECTION && !ACTIVE_STACK_OVERFLOW_CHECK
static_assert(false, "This target must enable active stack overflow check.");
#endif

#if !ACTIVE_STACK_OVERFLOW_CHECK && MAX_WASM_STACKSIZE_BEFORE_NATIVE_CALL == 0
#undef MAX_WASM_STACKSIZE_BEFORE_NATIVE_CALL
#define MAX_WASM_STACKSIZE_BEFORE_NATIVE_CALL 1000000U
#endif

#if ACTIVE_STACK_OVERFLOW_CHECK && STACKSIZE_LEFT_BEFORE_NATIVE_CALL == 0
#undef STACKSIZE_LEFT_BEFORE_NATIVE_CALL
#define STACKSIZE_LEFT_BEFORE_NATIVE_CALL 4096U
#endif

#if MAX_WASM_STACKSIZE_BEFORE_NATIVE_CALL != 0 && ACTIVE_STACK_OVERFLOW_CHECK
#warning "MAX_WASM_STACKSIZE_BEFORE_NATIVE_CALL is set but will not be used (Active stack overflow protection is enabled)"
#elif STACKSIZE_LEFT_BEFORE_NATIVE_CALL != 0 && !ACTIVE_STACK_OVERFLOW_CHECK
#warning "STACKSIZE_LEFT_BEFORE_NATIVE_CALL is set but will not be used (Active stack overflow protection is disabled)"
#endif

// ----------------------------------------------------------------------------
#if (((defined _WIN32) || (defined __linux__) || (defined __APPLE__) || defined(__unix__) || defined(__unix)) && !(defined __MINGW32__))
#define TARGET_SUPPORTS_PASSIVE_LINEAR_MEMORY_PROTECTION 1
#else
#define TARGET_SUPPORTS_PASSIVE_LINEAR_MEMORY_PROTECTION 0
#endif

#if !TARGET_SUPPORTS_PASSIVE_LINEAR_MEMORY_PROTECTION && !LINEAR_MEMORY_BOUNDS_CHECKS
#undef LINEAR_MEMORY_BOUNDS_CHECKS
#define LINEAR_MEMORY_BOUNDS_CHECKS 1
#endif

#if TARGET_SUPPORTS_PASSIVE_LINEAR_MEMORY_PROTECTION && LINEAR_MEMORY_BOUNDS_CHECKS && (TARGET_RTOS == 0)
#ifndef NO_PASSIVE_PROTECTION_WARNING
// coverity[autosar_cpp14_a16_7_1_violation]
#pragma message("Your target seems to support passive linear memory protection. Enabling linear memory bounds check will degrade performance.")
#endif
#endif

#if !TARGET_SUPPORTS_PASSIVE_LINEAR_MEMORY_PROTECTION && !LINEAR_MEMORY_BOUNDS_CHECKS
static_assert(false, "This target must enable linear memory bounds check.");
#endif
// ----------------------------------------------------------------------------

#if (defined __cplusplus) && (__cplusplus >= 201703L)
#define VB_IFCONSTEXPR constexpr
#else
#define VB_IFCONSTEXPR
#endif

#if !(defined VALGRIND)
#define NVALGRIND
#endif

#if (defined VB_DISABLE_NOEXCEPT)

#if !(defined VB_NOEXCEPT)
#define VB_NOEXCEPT
#endif

#if !(defined VB_THROW)
#define VB_THROW
#endif

#else

#if !(defined VB_NOEXCEPT)
#define VB_NOEXCEPT noexcept
#endif

#if !(defined VB_THROW)
#define VB_THROW noexcept(false)
#endif

#endif

#endif // CONFIG_H

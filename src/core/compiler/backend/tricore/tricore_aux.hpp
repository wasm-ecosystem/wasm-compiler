///
/// @file tricore_aux.hpp
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
#ifndef TRICORE_AUX_HPP
#define TRICORE_AUX_HPP

#include <array>
#include <cstdint>

#include "src/config.hpp"

namespace vb {
namespace tc {
namespace aux {

/// @brief Soft implementation of the cmp.f TriCore instruction
/// @param a Input value
/// @param b Input value
/// @return Result value
uint32_t cmpf(float const a, float const b) VB_NOEXCEPT;

/// @brief Soft implementation of the cmpd.f TriCore instruction
/// @param a Input value
/// @param b Input value
/// @return Result value
uint32_t cmpdf(double const a, double const b) VB_NOEXCEPT;

/// @brief Soft implementation of the div64 TriCore instruction
/// @param a Input value
/// @param b Input value
/// @return Result value
int64_t i64div_s(int64_t const a, int64_t const b) VB_NOEXCEPT;

/// @brief Soft implementation of the div64.u TriCore instruction
/// @param a Input value
/// @param b Input value
/// @return Result value
uint64_t i64div_u(uint64_t const a, uint64_t const b) VB_NOEXCEPT;

/// @brief Soft implementation of the rem64 TriCore instruction
/// @param a Input value
/// @param b Input value
/// @return Result value
int64_t i64rem_s(int64_t const a, int64_t const b) VB_NOEXCEPT;

/// @brief Soft implementation of the rem64.u TriCore instruction
/// @param a Input value
/// @param b Input value
/// @return Result value
uint64_t i64rem_u(uint64_t const a, uint64_t const b) VB_NOEXCEPT;

/// @brief Soft implementation of a 64-bit left shift
/// @param a Input value
/// @param b Input value
/// @return Result value
uint64_t i64shl(uint64_t const a, uint64_t b) VB_NOEXCEPT;

/// @brief Soft implementation of a 64-bit arithmetic right shift
/// @param a Input value
/// @param b Input value
/// @return Result value
uint64_t i64shr_s(uint64_t const a, uint64_t b) VB_NOEXCEPT;

/// @brief Soft implementation of a 64-bit logical right shift
/// @param a Input value
/// @param b Input value
/// @return Result value
uint64_t i64shr_u(uint64_t const a, uint64_t b) VB_NOEXCEPT;

/// @brief Soft implementation of a 64-bit right rotation
/// @param a Input value
/// @param b Input value
/// @return Result value
uint64_t i64rotr(uint64_t const a, uint64_t b) VB_NOEXCEPT;

/// @brief Soft implementation of a 64-bit left rotation
/// @param a Input value
/// @param b Input value
/// @return Result value
uint64_t i64rotl(uint64_t const a, uint64_t b) VB_NOEXCEPT;

/// @brief Soft implementation of an f32 ceil (round) operation
/// @param a Input value
/// @return Result value
float f32ceil(float const a) VB_NOEXCEPT;

/// @brief Soft implementation of an f32 floor (round) operation
/// @param a Input value
/// @return Result value
float f32floor(float const a) VB_NOEXCEPT;

/// @brief Soft implementation of an f32 trunc (round) operation
/// @param a Input value
/// @return Result value
float f32trunc(float const a) VB_NOEXCEPT;

/// @brief Soft implementation of an f32 nearest (round) operation
/// @param a Input value
/// @return Result value
float f32nearest(float const a) VB_NOEXCEPT;

/// @brief Soft implementation of an f32 sqrt operation
/// @param a Input value
/// @return Result value
float f32sqrt(float const a) VB_NOEXCEPT;

/// @brief Soft implementation of the add.f TriCore instruction
/// @param a Input value
/// @param b Input value
/// @return Result value
float f32add(float const a, float const b) VB_NOEXCEPT;

/// @brief Soft implementation of the sub.f TriCore instruction
/// @param a Input value
/// @param b Input value
/// @return Result value
float f32sub(float const a, float const b) VB_NOEXCEPT;

/// @brief Soft implementation of the mul.f TriCore instruction
/// @param a Input value
/// @param b Input value
/// @return Result value
float f32mul(float const a, float const b);

/// @brief Soft implementation of the div.f TriCore instruction
/// @param a Input value
/// @param b Input value
/// @return Result value
float f32div(float const a, float const b) VB_NOEXCEPT;

/// @brief Soft implementation of the min.f TriCore instruction
/// @param a Input value
/// @param b Input value
/// @return Result value
float f32min(float const a, float const b) VB_NOEXCEPT;

/// @brief Soft implementation of the max.f TriCore instruction
/// @param a Input value
/// @param b Input value
/// @return Result value
float f32max(float const a, float const b) VB_NOEXCEPT;

/// @brief Soft implementation of an f64 floor (round) operation
/// @param a Input value
/// @return Result value
double f64ceil(double const a) VB_NOEXCEPT;

/// @brief Soft implementation of an f64 floor (round) Wasm operation
/// @param a Input value
/// @return Result value
double f64floor(double const a) VB_NOEXCEPT;

/// @brief Soft implementation of an f64 trunc (round) Wasm operation
/// @param a Input value
/// @return Result value
double f64trunc(double const a) VB_NOEXCEPT;

/// @brief Soft implementation of an f64 nearest (round) Wasm operation
/// @param a Input value
/// @return Result value
double f64nearest(double const a) VB_NOEXCEPT;

/// @brief Soft implementation of an f64 sqrt (round) Wasm operation
/// @param a Input value
/// @return Result value
double f64sqrt(double const a) VB_NOEXCEPT;

/// @brief Soft implementation of the add.df TriCore instruction
/// @param a Input value
/// @param b Input value
/// @return Result value
double f64add(double const a, double const b) VB_NOEXCEPT;

/// @brief Soft implementation of the sub.df TriCore instruction
/// @param a Input value
/// @param b Input value
/// @return Result value
double f64sub(double const a, double const b) VB_NOEXCEPT;

/// @brief Soft implementation of the mul.df TriCore instruction
/// @param a Input value
/// @param b Input value
/// @return Result value
double f64mul(double const a, double const b) VB_NOEXCEPT;

/// @brief Soft implementation of the div.df TriCore instruction
/// @param a Input value
/// @param b Input value
/// @return Result value
double f64div(double const a, double const b) VB_NOEXCEPT;

/// @brief Soft implementation of the min.df TriCore instruction
/// @param a Input value
/// @param b Input value
/// @return Result value
double f64min(double const a, double const b) VB_NOEXCEPT;

/// @brief Soft implementation of the max.df TriCore instruction
/// @param a Input value
/// @param b Input value
/// @return Result value
double f64max(double const a, double const b) VB_NOEXCEPT;

/// @brief Soft implementation of the ftoiz TriCore instruction
/// @param a Input value
/// @return Result value
int32_t i32trunc_s_f32(float const a) VB_NOEXCEPT;

/// @brief Soft implementation of the ftouz TriCore instruction
/// @param a Input value
/// @return Result value
uint32_t i32trunc_u_f32(float const a) VB_NOEXCEPT;

/// @brief Soft implementation of an f32 to i64 conversion operation
/// @param a Input value
/// @return Result value
int64_t i64trunc_s_f32(float const a) VB_NOEXCEPT;

/// @brief Soft implementation of an f32 to u64 conversion operation
/// @param a Input value
/// @return Result value
uint64_t i64trunc_u_f32(float const a) VB_NOEXCEPT;

/// @brief Soft implementation of the dftoiz TriCore instruction
/// @param a Input value
/// @return Result value
int32_t i32trunc_s_f64(double const a) VB_NOEXCEPT;

/// @brief Soft implementation of the dftouz TriCore instruction
/// @param a Input value
/// @return Result value
uint32_t i32trunc_u_f64(double const a) VB_NOEXCEPT;

/// @brief Soft implementation of an i64 to f64 conversion operation
/// @param a Input value
/// @return Result value
int64_t i64trunc_s_f64(double const a) VB_NOEXCEPT;

/// @brief Soft implementation of an u64 to f64 conversion operation
/// @param a Input value
/// @return Result value
uint64_t i64trunc_u_f64(double const a) VB_NOEXCEPT;

/// @brief Soft implementation of the itof TriCore instruction
/// @param a Input value
/// @return Result value
float f32convert_s_i32(int32_t const a) VB_NOEXCEPT;

/// @brief Soft implementation of the utof TriCore instruction
/// @param a Input value
/// @return Result value
float f32convert_u_i32(uint32_t const a) VB_NOEXCEPT;

/// @brief Soft implementation of an f32 to i64 conversion operation
/// @param a Input value
/// @return Result value
float f32convert_s_i64(int64_t const a) VB_NOEXCEPT;

/// @brief Soft implementation of an f32 to u64 conversion operation
/// @param a Input value
/// @return Result value
float f32convert_u_i64(uint64_t const a) VB_NOEXCEPT;

/// @brief Soft implementation of the dftof TriCore instruction
/// @param a Input value
/// @return Result value
float f32demote_f64(double const a) VB_NOEXCEPT;

/// @brief Soft implementation of the itodf TriCore instruction
/// @param a Input value
/// @return Result value
double f64convert_s_i32(int32_t const a) VB_NOEXCEPT;

/// @brief Soft implementation of the utodf TriCore instruction
/// @param a Input value
/// @return Result value
double f64convert_u_i32(uint32_t const a) VB_NOEXCEPT;

/// @brief Soft implementation of an i64 to f64 conversion operation
/// @param a Input value
/// @return Result value
double f64convert_s_i64(int64_t const a) VB_NOEXCEPT;

/// @brief Soft implementation of an u64 to f64 conversion operation
/// @param a Input value
/// @return Result value
double f64convert_u_i64(uint64_t const a) VB_NOEXCEPT;

/// @brief Soft implementation of the ftodf TriCore instruction
/// @param a Input value
/// @return Result value
double f64promote_f32(float const a) VB_NOEXCEPT;

///
/// @brief Enum for quick calling and identification of softfloat implementations
///
enum class MappedFncs : uint8_t {
  CMPF,
  CMPDF,
  I64_DIV_S,
  I64_DIV_U,
  I64_REM_S,
  I64_REM_U,
  I64_SHL,
  I64_SHR_S,
  I64_SHR_U,
  I64_ROTR,
  I64_ROTL,
  F32_CEIL,
  F32_FLOOR,
  F32_TRUNC,
  F32_NEAREST,
  F32_SQRT,
  F32_ADD,
  F32_SUB,
  F32_MUL,
  F32_DIV,
  F32_MIN,
  F32_MAX,
  F64_CEIL,
  F64_FLOOR,
  F64_TRUNC,
  F64_NEAREST,
  F64_SQRT,
  F64_ADD,
  F64_SUB,
  F64_MUL,
  F64_DIV,
  F64_MIN,
  F64_MAX,
  I32_TRUNC_F32_S,
  I32_TRUNC_F32_U,
  I64_TRUNC_F32_S,
  I64_TRUNC_F32_U,
  I32_TRUNC_F64_S,
  I32_TRUNC_F64_U,
  I64_TRUNC_F64_S,
  I64_TRUNC_F64_U,
  F32_CONVERT_I32_S,
  F32_CONVERT_I32_U,
  F32_CONVERT_I64_S,
  F32_CONVERT_I64_U,
  F32_DEMOTE_F64,
  F64_CONVERT_I32_S,
  F64_CONVERT_I32_U,
  F64_CONVERT_I64_S,
  F64_CONVERT_I64_U,
  F64_PROMOTE_F32
};

#if TC_LINK_AUX_FNCS_DYNAMICALLY
///
/// @brief Get array of softfloat implementation functions
///
std::array<uint32_t, 51U> const &getSoftfloatImplementationFunctions() VB_NOEXCEPT;
#else
///
/// @brief Get pointer/address of softfloat implementation function
///
/// @param fnc Mapped function
///
/// @return Pointer of softfloat implementation function casted to uint32_t (to enable cross-compilation)
///
uint32_t getSoftfloatImplementationFunctionPtr(MappedFncs const fnc) VB_NOEXCEPT;
#endif

} // namespace aux
} // namespace tc
} // namespace vb

#endif

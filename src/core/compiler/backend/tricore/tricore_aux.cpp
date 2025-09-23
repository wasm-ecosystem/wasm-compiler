///
/// @file tricore_aux.cpp
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
// coverity[autosar_cpp14_a16_2_2_violation]
#include "src/config.hpp"
#ifdef JIT_TARGET_TRICORE
#include <array>
#include <cmath>
#include <cstdint>

#include "tricore_aux.hpp"

#include "src/core/common/util.hpp"
extern "C" {
// coverity[autosar_cpp14_m16_0_1_violation]
#include "thirdparty/berkeley-softfloat-3/source/include/softfloat.h"
#include "thirdparty/berkeley-softfloat-3/source/include/softfloat_types.h"
}

#ifndef F2V
#define F2V(FNC) (static_cast<uint32_t>(pToNum(FNC)))
#endif

namespace vb {
namespace tc {
namespace aux {
static_assert(sizeof(double) == 8, "Double size not suitable");

#if TC_LINK_AUX_FNCS_DYNAMICALLY
///
/// @brief List of all soft-linked functions
///
// coverity[autosar_cpp14_m3_4_1_violation]
const std::array<uint32_t, 51U> fncArr{{F2V(&cmpf),
                                        F2V(&cmpdf),
                                        F2V(&i64div_s),
                                        F2V(&i64div_u),
                                        F2V(&i64rem_s),
                                        F2V(&i64rem_u),
                                        F2V(&i64shl),
                                        F2V(&i64shr_s),
                                        F2V(&i64shr_u),
                                        F2V(&i64rotr),
                                        F2V(&i64rotl),
                                        F2V(&f32ceil),
                                        F2V(&f32floor),
                                        F2V(&f32trunc),
                                        F2V(&f32nearest),
                                        F2V(&f32sqrt),
                                        F2V(&f32add),
                                        F2V(&f32sub),
                                        F2V(&f32mul),
                                        F2V(&f32div),
                                        F2V(&f32min),
                                        F2V(&f32max),
                                        F2V(&f64ceil),
                                        F2V(&f64floor),
                                        F2V(&f64trunc),
                                        F2V(&f64nearest),
                                        F2V(&f64sqrt),
                                        F2V(&f64add),
                                        F2V(&f64sub),
                                        F2V(&f64mul),
                                        F2V(&f64div),
                                        F2V(&f64min),
                                        F2V(&f64max),
                                        F2V(&i32trunc_s_f32),
                                        F2V(&i32trunc_u_f32),
                                        F2V(&i64trunc_s_f32),
                                        F2V(&i64trunc_u_f32),
                                        F2V(&i32trunc_s_f64),
                                        F2V(&i32trunc_u_f64),
                                        F2V(&i64trunc_s_f64),
                                        F2V(&i64trunc_u_f64),
                                        F2V(&f32convert_s_i32),
                                        F2V(&f32convert_u_i32),
                                        F2V(&f32convert_s_i64),
                                        F2V(&f32convert_u_i64),
                                        F2V(&f32demote_f64),
                                        F2V(&f64convert_s_i32),
                                        F2V(&f64convert_u_i32),
                                        F2V(&f64convert_s_i64),
                                        F2V(&f64convert_u_i64),
                                        F2V(&f64promote_f32)}};

std::array<uint32_t, 51U> const &getSoftfloatImplementationFunctions() VB_NOEXCEPT {
  return fncArr;
}
#else
uint32_t getSoftfloatImplementationFunctionPtr(MappedFncs const fnc) VB_NOEXCEPT {
  switch (fnc) {
  case MappedFncs::CMPF:
    return F2V(cmpf);
  case MappedFncs::CMPDF:
    return F2V(cmpdf);
  case MappedFncs::I64_DIV_S:
    return F2V(i64div_s);
  case MappedFncs::I64_DIV_U:
    return F2V(i64div_u);
  case MappedFncs::I64_REM_S:
    return F2V(i64rem_s);
  case MappedFncs::I64_REM_U:
    return F2V(i64rem_u);
  case MappedFncs::I64_SHL:
    return F2V(i64shl);
  case MappedFncs::I64_SHR_S:
    return F2V(i64shr_s);
  case MappedFncs::I64_SHR_U:
    return F2V(i64shr_u);
  case MappedFncs::I64_ROTR:
    return F2V(i64rotr);
  case MappedFncs::I64_ROTL:
    return F2V(i64rotl);
  case MappedFncs::F32_CEIL:
    return F2V(f32ceil);
  case MappedFncs::F32_FLOOR:
    return F2V(f32floor);
  case MappedFncs::F32_TRUNC:
    return F2V(f32trunc);
  case MappedFncs::F32_NEAREST:
    return F2V(f32nearest);
  case MappedFncs::F32_SQRT:
    return F2V(f32sqrt);
  case MappedFncs::F32_ADD:
    return F2V(f32add);
  case MappedFncs::F32_SUB:
    return F2V(f32sub);
  case MappedFncs::F32_MUL:
    return F2V(f32mul);
  case MappedFncs::F32_DIV:
    return F2V(f32div);
  case MappedFncs::F32_MIN:
    return F2V(f32min);
  case MappedFncs::F32_MAX:
    return F2V(f32max);
  case MappedFncs::F64_CEIL:
    return F2V(f64ceil);
  case MappedFncs::F64_FLOOR:
    return F2V(f64floor);
  case MappedFncs::F64_TRUNC:
    return F2V(f64trunc);
  case MappedFncs::F64_NEAREST:
    return F2V(f64nearest);
  case MappedFncs::F64_SQRT:
    return F2V(f64sqrt);
  case MappedFncs::F64_ADD:
    return F2V(f64add);
  case MappedFncs::F64_SUB:
    return F2V(f64sub);
  case MappedFncs::F64_MUL:
    return F2V(f64mul);
  case MappedFncs::F64_DIV:
    return F2V(f64div);
  case MappedFncs::F64_MIN:
    return F2V(f64min);
  case MappedFncs::F64_MAX:
    return F2V(f64max);
  case MappedFncs::I32_TRUNC_F32_S:
    return F2V(i32trunc_s_f32);
  case MappedFncs::I32_TRUNC_F32_U:
    return F2V(i32trunc_u_f32);
  case MappedFncs::I64_TRUNC_F32_S:
    return F2V(i64trunc_s_f32);
  case MappedFncs::I64_TRUNC_F32_U:
    return F2V(i64trunc_u_f32);
  case MappedFncs::I32_TRUNC_F64_S:
    return F2V(i32trunc_s_f64);
  case MappedFncs::I32_TRUNC_F64_U:
    return F2V(i32trunc_u_f64);
  case MappedFncs::I64_TRUNC_F64_S:
    return F2V(i64trunc_s_f64);
  case MappedFncs::I64_TRUNC_F64_U:
    return F2V(i64trunc_u_f64);
  case MappedFncs::F32_CONVERT_I32_S:
    return F2V(f32convert_s_i32);
  case MappedFncs::F32_CONVERT_I32_U:
    return F2V(f32convert_u_i32);
  case MappedFncs::F32_CONVERT_I64_S:
    return F2V(f32convert_s_i64);
  case MappedFncs::F32_CONVERT_I64_U:
    return F2V(f32convert_u_i64);
  case MappedFncs::F32_DEMOTE_F64:
    return F2V(f32demote_f64);
  case MappedFncs::F64_CONVERT_I32_S:
    return F2V(f64convert_s_i32);
  case MappedFncs::F64_CONVERT_I32_U:
    return F2V(f64convert_u_i32);
  case MappedFncs::F64_CONVERT_I64_S:
    return F2V(f64convert_s_i64);
  case MappedFncs::F64_CONVERT_I64_U:
    return F2V(f64convert_u_i64);
  case MappedFncs::F64_PROMOTE_F32:
    return F2V(f64promote_f32);
  default:
    return 0U;
  }
}
#endif

uint32_t cmpf(float const a, float const b) VB_NOEXCEPT {
  bool const lt{std::isless(a, b)};
  bool const gt{std::isgreater(a, b)};
  bool const unordered{std::isnan(a) || std::isnan(b)};

  bool const eq{((!gt) && (!lt)) && (!unordered)};

  bool const a_subnormal{std::fpclassify(a) == FP_SUBNORMAL};
  bool const b_subnormal{std::fpclassify(b) == FP_SUBNORMAL};

  uint32_t const res{(lt ? 1_U32 : 0_U32) | (eq ? 2_U32 : 0_U32) | (gt ? 4_U32 : 0_U32) | (unordered ? 8_U32 : 0_U32) |
                     (a_subnormal ? 16_U32 : 0_U32) | (b_subnormal ? 32_U32 : 0_U32)};
  return res;
}

uint32_t cmpdf(double const a, double const b) VB_NOEXCEPT {
  bool const lt{std::isless(a, b)};
  bool const gt{std::isgreater(a, b)};
  bool const unordered{std::isnan(a) || std::isnan(b)};

  bool const eq{((!gt) && (!lt)) && (!unordered)};

  bool const a_subnormal{std::fpclassify(a) == FP_SUBNORMAL};
  bool const b_subnormal{std::fpclassify(b) == FP_SUBNORMAL};

  uint32_t const res{(lt ? 1_U32 : 0_U32) | (eq ? 2_U32 : 0_U32) | (gt ? 4_U32 : 0_U32) | (unordered ? 8_U32 : 0_U32) |
                     (a_subnormal ? 16_U32 : 0_U32) | (b_subnormal ? 32_U32 : 0_U32)};
  return res;
}

int64_t i64div_s(int64_t const a, int64_t const b) VB_NOEXCEPT {
  // coverity[autosar_cpp14_a5_6_1_violation] b==0 checked by JIT code
  return a / b;
}
uint64_t i64div_u(uint64_t const a, uint64_t const b) VB_NOEXCEPT {
  // coverity[autosar_cpp14_a5_6_1_violation] b==0 checked by JIT code
  return a / b;
}
int64_t i64rem_s(int64_t const a, int64_t const b) VB_NOEXCEPT {
  // coverity[autosar_cpp14_a5_6_1_violation] b==0 checked by JIT code
  return a % b;
}
uint64_t i64rem_u(uint64_t const a, uint64_t const b) VB_NOEXCEPT {
  // coverity[autosar_cpp14_a5_6_1_violation] b==0 checked by JIT code
  return a % b;
}

uint64_t i64shl(uint64_t const a, uint64_t b) VB_NOEXCEPT {
  b &= 0x3FLLU;
  return a << b;
}
uint64_t i64shr_s(uint64_t const a, uint64_t b) VB_NOEXCEPT {
  b &= 0x3FLLU;
  return (bit_cast<int64_t>(a) < 0LL) ? ~(~a >> b) : (a >> b);
}
uint64_t i64shr_u(uint64_t const a, uint64_t b) VB_NOEXCEPT {
  b &= 0x3FLLU;
  return a >> b;
}

uint64_t i64rotr(uint64_t const a, uint64_t b) VB_NOEXCEPT {
  b &= 0x3FLLU;
  return (a >> b) | (a << (64U - b));
}

uint64_t i64rotl(uint64_t const a, uint64_t b) VB_NOEXCEPT {
  b &= 0x3FLLU;
  return (a << b) | (a >> (64U - b));
}

float f32ceil(float const a) VB_NOEXCEPT {
  float32_t const val{bit_cast<uint32_t>(a)};
  return bit_cast<float>(f32_roundToInt(val, softfloat_round_max, false).v);
}
float f32floor(float const a) VB_NOEXCEPT {
  float32_t const val{bit_cast<uint32_t>(a)};
  return bit_cast<float>(f32_roundToInt(val, softfloat_round_min, false).v);
}
float f32trunc(float const a) VB_NOEXCEPT {
  float32_t const val{bit_cast<uint32_t>(a)};
  return bit_cast<float>(f32_roundToInt(val, softfloat_round_minMag, false).v);
}
float f32nearest(float const a) VB_NOEXCEPT {
  float32_t const val{bit_cast<uint32_t>(a)};
  return bit_cast<float>(f32_roundToInt(val, softfloat_round_near_even, false).v);
}
float f32sqrt(float const a) VB_NOEXCEPT {
  float32_t const val{bit_cast<uint32_t>(a)};
  return bit_cast<float>(f32_sqrt(val).v);
}
float f32add(float const a, float const b) VB_NOEXCEPT {
  float32_t const valA{bit_cast<uint32_t>(a)};
  float32_t const valB{bit_cast<uint32_t>(b)};
  return bit_cast<float>(f32_add(valA, valB).v);
}
float f32sub(float const a, float const b) VB_NOEXCEPT {
  float32_t const valA{bit_cast<uint32_t>(a)};
  float32_t const valB{bit_cast<uint32_t>(b)};
  return bit_cast<float>(f32_sub(valA, valB).v);
}
float f32mul(float const a, float const b) {
  float32_t const valA{bit_cast<uint32_t>(a)};
  float32_t const valB{bit_cast<uint32_t>(b)};
  return bit_cast<float>(f32_mul(valA, valB).v);
}
float f32div(float const a, float const b) VB_NOEXCEPT {
  float32_t const valA{bit_cast<uint32_t>(a)};
  float32_t const valB{bit_cast<uint32_t>(b)};
  return bit_cast<float>(f32_div(valA, valB).v);
}
float f32min(float const a, float const b) VB_NOEXCEPT {
  float32_t const valA{bit_cast<uint32_t>(a)};
  float32_t const valB{bit_cast<uint32_t>(b)};
  if (std::isnan(a)) {
    return bit_cast<float>(valA.v | (1_U32 << 22_U32));
  }
  if (std::isnan(b)) {
    return bit_cast<float>(valB.v | (1_U32 << 22_U32));
  }

  if (((valA.v << 1U) == 0U) && ((valB.v << 1U) == 0U)) {
    return bit_cast<float>(valA.v | valB.v);
  }
  bool const aLTb{f32_lt(valA, valB)};
  return aLTb ? a : b;
}
float f32max(float const a, float const b) VB_NOEXCEPT {
  float32_t const valA{bit_cast<uint32_t>(a)};
  float32_t const valB{bit_cast<uint32_t>(b)};
  if (std::isnan(a)) {
    return bit_cast<float>(bit_cast<uint32_t>(a) | (1_U32 << 22_U32));
  }
  if (std::isnan(b)) {
    return bit_cast<float>(bit_cast<uint32_t>(b) | (1_U32 << 22_U32));
  }

  if (((valA.v << 1U) == 0U) && ((valB.v << 1U) == 0U)) {
    return bit_cast<float>(valA.v & valB.v);
  }
  bool const aLTb{f32_lt(valA, valB)};
  return aLTb ? b : a;
}

//
//
//

double f64ceil(double const a) VB_NOEXCEPT {
  float64_t const val{bit_cast<uint64_t>(a)};
  return bit_cast<double>(f64_roundToInt(val, softfloat_round_max, false).v);
}
double f64floor(double const a) VB_NOEXCEPT {
  float64_t const val{bit_cast<uint64_t>(a)};
  return bit_cast<double>(f64_roundToInt(val, softfloat_round_min, false).v);
}
double f64trunc(double const a) VB_NOEXCEPT {
  float64_t const val{bit_cast<uint64_t>(a)};
  return bit_cast<double>(f64_roundToInt(val, softfloat_round_minMag, false).v);
}
double f64nearest(double const a) VB_NOEXCEPT {
  float64_t const val{bit_cast<uint64_t>(a)};
  return bit_cast<double>(f64_roundToInt(val, softfloat_round_near_even, false).v);
}
double f64sqrt(double const a) VB_NOEXCEPT {
  float64_t const val{bit_cast<uint64_t>(a)};
  return bit_cast<double>(f64_sqrt(val).v);
}
double f64add(double const a, double const b) VB_NOEXCEPT {
  float64_t const valA{bit_cast<uint64_t>(a)};
  float64_t const valB{bit_cast<uint64_t>(b)};
  return bit_cast<double>(f64_add(valA, valB).v);
}
double f64sub(double const a, double const b) VB_NOEXCEPT {
  float64_t const valA{bit_cast<uint64_t>(a)};
  float64_t const valB{bit_cast<uint64_t>(b)};
  return bit_cast<double>(f64_sub(valA, valB).v);
}
double f64mul(double const a, double const b) VB_NOEXCEPT {
  float64_t const valA{bit_cast<uint64_t>(a)};
  float64_t const valB{bit_cast<uint64_t>(b)};
  return bit_cast<double>(f64_mul(valA, valB).v);
}
double f64div(double const a, double const b) VB_NOEXCEPT {
  float64_t const valA{bit_cast<uint64_t>(a)};
  float64_t const valB{bit_cast<uint64_t>(b)};
  return bit_cast<double>(f64_div(valA, valB).v);
}
double f64min(double const a, double const b) VB_NOEXCEPT {
  float64_t const valA{bit_cast<uint64_t>(a)};
  float64_t const valB{bit_cast<uint64_t>(b)};
  if (std::isnan(a)) {
    return bit_cast<double>(valA.v | (1_U64 << 51_U64));
  }
  if (std::isnan(b)) {
    return bit_cast<double>(valB.v | (1_U64 << 51_U64));
  }

  if (((valA.v << 1U) == 0U) && ((valB.v << 1U) == 0U)) {
    return bit_cast<double>(valA.v | valB.v);
  }
  bool const aLTb{f64_lt(valA, valB)};
  return aLTb ? a : b;
}
double f64max(double const a, double const b) VB_NOEXCEPT {
  float64_t const valA{bit_cast<uint64_t>(a)};
  float64_t const valB{bit_cast<uint64_t>(b)};
  if (std::isnan(a)) {
    return bit_cast<double>(valA.v | (1_U64 << 51_U64));
  }
  if (std::isnan(b)) {
    return bit_cast<double>(valB.v | (1_U64 << 51_U64));
  }

  if (((valA.v << 1U) == 0U) && ((valB.v << 1U) == 0U)) {
    return bit_cast<double>(valA.v & valB.v);
  }
  bool const aLTb{f64_lt(valA, valB)};
  return aLTb ? b : a;
}

//
//
//

int32_t i32trunc_s_f32(float const a) VB_NOEXCEPT {
  float32_t const val{bit_cast<uint32_t>(a)};
  return static_cast<int32_t>(f32_to_i32(val, softfloat_round_minMag, false));
}
uint32_t i32trunc_u_f32(float const a) VB_NOEXCEPT {
  float32_t const val{bit_cast<uint32_t>(a)};
  return static_cast<uint32_t>(f32_to_ui32(val, softfloat_round_minMag, false));
}
int64_t i64trunc_s_f32(float const a) VB_NOEXCEPT {
  float32_t const val{bit_cast<uint32_t>(a)};
  return f32_to_i64(val, softfloat_round_minMag, false);
}
uint64_t i64trunc_u_f32(float const a) VB_NOEXCEPT {
  float32_t const val{bit_cast<uint32_t>(a)};
  return f32_to_ui64(val, softfloat_round_minMag, false);
}

int32_t i32trunc_s_f64(double const a) VB_NOEXCEPT {
  float64_t const val{bit_cast<uint64_t>(a)};
  return static_cast<int32_t>(f64_to_i32(val, softfloat_round_minMag, false));
}
uint32_t i32trunc_u_f64(double const a) VB_NOEXCEPT {
  float64_t const val{bit_cast<uint64_t>(a)};
  return static_cast<uint32_t>(f64_to_ui32(val, softfloat_round_minMag, false));
}
int64_t i64trunc_s_f64(double const a) VB_NOEXCEPT {
  float64_t const val{bit_cast<uint64_t>(a)};
  return f64_to_i64(val, softfloat_round_minMag, false);
}
uint64_t i64trunc_u_f64(double const a) VB_NOEXCEPT {
  float64_t const val{bit_cast<uint64_t>(a)};
  return f64_to_ui64(val, softfloat_round_minMag, false);
}

float f32convert_s_i32(int32_t const a) VB_NOEXCEPT {
  return bit_cast<float>(i32_to_f32(a).v);
}
float f32convert_u_i32(uint32_t const a) VB_NOEXCEPT {
  return bit_cast<float>(ui32_to_f32(a).v);
}
float f32convert_s_i64(int64_t const a) VB_NOEXCEPT {
  return bit_cast<float>(i64_to_f32(a).v);
}
float f32convert_u_i64(uint64_t const a) VB_NOEXCEPT {
  return bit_cast<float>(ui64_to_f32(a).v);
}
float f32demote_f64(double const a) VB_NOEXCEPT {
  float64_t const val{bit_cast<uint64_t>(a)};
  return bit_cast<float>(f64_to_f32(val).v);
}

double f64convert_s_i32(int32_t const a) VB_NOEXCEPT {
  return bit_cast<double>(i32_to_f64(a).v);
}
double f64convert_u_i32(uint32_t const a) VB_NOEXCEPT {
  return bit_cast<double>(ui32_to_f64(a).v);
}
double f64convert_s_i64(int64_t const a) VB_NOEXCEPT {
  return bit_cast<double>(i64_to_f64(a).v);
}
double f64convert_u_i64(uint64_t const a) VB_NOEXCEPT {
  return bit_cast<double>(ui64_to_f64(a).v);
}
double f64promote_f32(float const a) VB_NOEXCEPT {
  float32_t const val{bit_cast<uint32_t>(a)};
  return bit_cast<double>(f32_to_f64(val).v);
}

} // namespace aux
} // namespace tc
} // namespace vb
#endif

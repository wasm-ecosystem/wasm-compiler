/*
 * Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
 * SPDX-License-Identifier: Apache-2.0
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef TESTIMPORTS_HPP
#define TESTIMPORTS_HPP

#include <cstdint>
#include <iostream>
#include <vector>

#include "src/WasmModule/WasmModule.hpp"
#include "src/core/common/NativeSymbol.hpp"
#include "src/core/common/Span.hpp"
#include "src/core/common/TrapCode.hpp"
#include "src/core/common/function_traits.hpp"
#include "src/core/common/util.hpp"
#include "src/core/runtime/ImportFunctionV2.hpp"

namespace spectest {
class ImportsMaker {
public:
  static inline void nop(void *const ctx) noexcept {
    static_cast<void>(ctx);
  }

  static inline void func_i64_i64(uint64_t I1, uint64_t I2, void *const ctx) noexcept {
    static_cast<void>(ctx);
    static_cast<void>(I1);
    static_cast<void>(I2);
  }

  static inline uint32_t sumI(uint32_t i1, uint32_t i2, uint32_t i3, uint32_t i4, uint32_t i5, uint32_t i6, uint32_t i7, uint32_t i8, uint32_t i9,
                              uint32_t i10, uint32_t i11, uint32_t i12, uint32_t i13, uint32_t i14, uint32_t i15, uint32_t i16, uint32_t i17,
                              uint32_t i18, uint32_t i19, uint32_t i20, uint32_t i21, uint32_t i22, uint32_t i23, uint32_t i24, uint32_t i25,
                              uint32_t i26, uint32_t i27, uint32_t i28, uint32_t i29, uint32_t i30, uint32_t i31, uint32_t i32, uint32_t i33,
                              uint32_t i34, uint32_t i35, uint32_t i36, uint32_t i37, uint32_t i38, uint32_t i39, uint32_t i40, uint32_t i41,
                              uint32_t i42, uint32_t i43, uint32_t i44, uint32_t i45, uint32_t i46, uint32_t i47, uint32_t i48, uint32_t i49,
                              uint32_t i50, void *const ctx) noexcept {
    static_cast<void>(ctx);
    return i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8 + i9 + i10 + i11 + i12 + i13 + i14 + i15 + i16 + i17 + i18 + i19 + i20 + i21 + i22 + i23 + i24 +
           i25 + i26 + i27 + i28 + i29 + i30 + i31 + i32 + i33 + i34 + i35 + i36 + i37 + i38 + i39 + i40 + i41 + i42 + i43 + i44 + i45 + i46 + i47 +
           i48 + i49 + i50;
  }

  static inline uint32_t sumLastI(float f1, float f2, float f3, float f4, float f5, float f6, float f7, float f8, float f9, float f10, float f11,
                                  float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19, float f20, float f21,
                                  float f22, float f23, float f24, float f25, float f26, float f27, float f28, float f29, float f30, float f31,
                                  float f32, float f33, float f34, float f35, float f36, float f37, float f38, float f39, float f40, float f41,
                                  float f42, float f43, float f44, float f45, float f46, float f47, float f48, float f49, float f50, uint32_t i1,
                                  uint32_t i2, uint32_t i3, uint32_t i4, uint32_t i5, uint32_t i6, uint32_t i7, uint32_t i8, uint32_t i9,
                                  uint32_t i10, uint32_t i11, uint32_t i12, uint32_t i13, uint32_t i14, uint32_t i15, uint32_t i16, uint32_t i17,
                                  uint32_t i18, uint32_t i19, uint32_t i20, uint32_t i21, uint32_t i22, uint32_t i23, uint32_t i24, uint32_t i25,
                                  uint32_t i26, uint32_t i27, uint32_t i28, uint32_t i29, uint32_t i30, uint32_t i31, uint32_t i32, uint32_t i33,
                                  uint32_t i34, uint32_t i35, uint32_t i36, uint32_t i37, uint32_t i38, uint32_t i39, uint32_t i40, uint32_t i41,
                                  uint32_t i42, uint32_t i43, uint32_t i44, uint32_t i45, uint32_t i46, uint32_t i47, uint32_t i48, uint32_t i49,
                                  uint32_t i50, void *const ctx) noexcept {
    static_cast<void>(ctx);
    static_cast<void>(f1);
    static_cast<void>(f2);
    static_cast<void>(f3);
    static_cast<void>(f4);
    static_cast<void>(f5);
    static_cast<void>(f6);
    static_cast<void>(f7);
    static_cast<void>(f8);
    static_cast<void>(f9);
    static_cast<void>(f10);
    static_cast<void>(f11);
    static_cast<void>(f12);
    static_cast<void>(f13);
    static_cast<void>(f14);
    static_cast<void>(f15);
    static_cast<void>(f16);
    static_cast<void>(f17);
    static_cast<void>(f18);
    static_cast<void>(f19);
    static_cast<void>(f20);
    static_cast<void>(f21);
    static_cast<void>(f22);
    static_cast<void>(f23);
    static_cast<void>(f24);
    static_cast<void>(f25);
    static_cast<void>(f26);
    static_cast<void>(f27);
    static_cast<void>(f28);
    static_cast<void>(f29);
    static_cast<void>(f30);
    static_cast<void>(f31);
    static_cast<void>(f32);
    static_cast<void>(f33);
    static_cast<void>(f34);
    static_cast<void>(f35);
    static_cast<void>(f36);
    static_cast<void>(f37);
    static_cast<void>(f38);
    static_cast<void>(f39);
    static_cast<void>(f40);
    static_cast<void>(f41);
    static_cast<void>(f42);
    static_cast<void>(f43);
    static_cast<void>(f44);
    static_cast<void>(f45);
    static_cast<void>(f46);
    static_cast<void>(f47);
    static_cast<void>(f48);
    static_cast<void>(f49);
    static_cast<void>(f50);

    return i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8 + i9 + i10 + i11 + i12 + i13 + i14 + i15 + i16 + i17 + i18 + i19 + i20 + i21 + i22 + i23 + i24 +
           i25 + i26 + i27 + i28 + i29 + i30 + i31 + i32 + i33 + i34 + i35 + i36 + i37 + i38 + i39 + i40 + i41 + i42 + i43 + i44 + i45 + i46 + i47 +
           i48 + i49 + i50;
  }

  static inline uint32_t sumMixedI(uint32_t i1, float f1, uint32_t i2, float f2, uint32_t i3, float f3, uint32_t i4, float f4, uint32_t i5, float f5,
                                   uint32_t i6, float f6, uint32_t i7, float f7, uint32_t i8, float f8, uint32_t i9, float f9, uint32_t i10,
                                   float f10, uint32_t i11, float f11, uint32_t i12, float f12, uint32_t i13, float f13, uint32_t i14, float f14,
                                   uint32_t i15, float f15, uint32_t i16, float f16, uint32_t i17, float f17, uint32_t i18, float f18, uint32_t i19,
                                   float f19, uint32_t i20, float f20, uint32_t i21, float f21, uint32_t i22, float f22, uint32_t i23, float f23,
                                   uint32_t i24, float f24, uint32_t i25, float f25, uint32_t i26, float f26, uint32_t i27, float f27, uint32_t i28,
                                   float f28, uint32_t i29, float f29, uint32_t i30, float f30, uint32_t i31, float f31, uint32_t i32, float f32,
                                   uint32_t i33, float f33, uint32_t i34, float f34, uint32_t i35, float f35, uint32_t i36, float f36, uint32_t i37,
                                   float f37, uint32_t i38, float f38, uint32_t i39, float f39, uint32_t i40, float f40, uint32_t i41, float f41,
                                   uint32_t i42, float f42, uint32_t i43, float f43, uint32_t i44, float f44, uint32_t i45, float f45, uint32_t i46,
                                   float f46, uint32_t i47, float f47, uint32_t i48, float f48, uint32_t i49, float f49, uint32_t i50, float f50,
                                   void *const ctx) noexcept {
    static_cast<void>(ctx);
    static_cast<void>(f1);
    static_cast<void>(f2);
    static_cast<void>(f3);
    static_cast<void>(f4);
    static_cast<void>(f5);
    static_cast<void>(f6);
    static_cast<void>(f7);
    static_cast<void>(f8);
    static_cast<void>(f9);
    static_cast<void>(f10);
    static_cast<void>(f11);
    static_cast<void>(f12);
    static_cast<void>(f13);
    static_cast<void>(f14);
    static_cast<void>(f15);
    static_cast<void>(f16);
    static_cast<void>(f17);
    static_cast<void>(f18);
    static_cast<void>(f19);
    static_cast<void>(f20);
    static_cast<void>(f21);
    static_cast<void>(f22);
    static_cast<void>(f23);
    static_cast<void>(f24);
    static_cast<void>(f25);
    static_cast<void>(f26);
    static_cast<void>(f27);
    static_cast<void>(f28);
    static_cast<void>(f29);
    static_cast<void>(f30);
    static_cast<void>(f31);
    static_cast<void>(f32);
    static_cast<void>(f33);
    static_cast<void>(f34);
    static_cast<void>(f35);
    static_cast<void>(f36);
    static_cast<void>(f37);
    static_cast<void>(f38);
    static_cast<void>(f39);
    static_cast<void>(f40);
    static_cast<void>(f41);
    static_cast<void>(f42);
    static_cast<void>(f43);
    static_cast<void>(f44);
    static_cast<void>(f45);
    static_cast<void>(f46);
    static_cast<void>(f47);
    static_cast<void>(f48);
    static_cast<void>(f49);
    static_cast<void>(f50);
    return i1 + i2 + i3 + i4 + i5 + i6 + i7 + i8 + i9 + i10 + i11 + i12 + i13 + i14 + i15 + i16 + i17 + i18 + i19 + i20 + i21 + i22 + i23 + i24 +
           i25 + i26 + i27 + i28 + i29 + i30 + i31 + i32 + i33 + i34 + i35 + i36 + i37 + i38 + i39 + i40 + i41 + i42 + i43 + i44 + i45 + i46 + i47 +
           i48 + i49 + i50;
  }

  static inline float sumF(float f1, float f2, float f3, float f4, float f5, float f6, float f7, float f8, float f9, float f10, float f11, float f12,
                           float f13, float f14, float f15, float f16, float f17, float f18, float f19, float f20, float f21, float f22, float f23,
                           float f24, float f25, float f26, float f27, float f28, float f29, float f30, float f31, float f32, float f33, float f34,
                           float f35, float f36, float f37, float f38, float f39, float f40, float f41, float f42, float f43, float f44, float f45,
                           float f46, float f47, float f48, float f49, float f50, void *const ctx) noexcept {
    static_cast<void>(ctx);
    return f1 + f2 + f3 + f4 + f5 + f6 + f7 + f8 + f9 + f10 + f11 + f12 + f13 + f14 + f15 + f16 + f17 + f18 + f19 + f20 + f21 + f22 + f23 + f24 +
           f25 + f26 + f27 + f28 + f29 + f30 + f31 + f32 + f33 + f34 + f35 + f36 + f37 + f38 + f39 + f40 + f41 + f42 + f43 + f44 + f45 + f46 + f47 +
           f48 + f49 + f50;
  }

  static inline float sumLastF(uint32_t i1, uint32_t i2, uint32_t i3, uint32_t i4, uint32_t i5, uint32_t i6, uint32_t i7, uint32_t i8, uint32_t i9,
                               uint32_t i10, uint32_t i11, uint32_t i12, uint32_t i13, uint32_t i14, uint32_t i15, uint32_t i16, uint32_t i17,
                               uint32_t i18, uint32_t i19, uint32_t i20, uint32_t i21, uint32_t i22, uint32_t i23, uint32_t i24, uint32_t i25,
                               uint32_t i26, uint32_t i27, uint32_t i28, uint32_t i29, uint32_t i30, uint32_t i31, uint32_t i32, uint32_t i33,
                               uint32_t i34, uint32_t i35, uint32_t i36, uint32_t i37, uint32_t i38, uint32_t i39, uint32_t i40, uint32_t i41,
                               uint32_t i42, uint32_t i43, uint32_t i44, uint32_t i45, uint32_t i46, uint32_t i47, uint32_t i48, uint32_t i49,
                               uint32_t i50, float f1, float f2, float f3, float f4, float f5, float f6, float f7, float f8, float f9, float f10,
                               float f11, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19, float f20,
                               float f21, float f22, float f23, float f24, float f25, float f26, float f27, float f28, float f29, float f30,
                               float f31, float f32, float f33, float f34, float f35, float f36, float f37, float f38, float f39, float f40,
                               float f41, float f42, float f43, float f44, float f45, float f46, float f47, float f48, float f49, float f50,
                               void *const ctx) noexcept {
    static_cast<void>(ctx);
    static_cast<void>(i1);
    static_cast<void>(i2);
    static_cast<void>(i3);
    static_cast<void>(i4);
    static_cast<void>(i5);
    static_cast<void>(i6);
    static_cast<void>(i7);
    static_cast<void>(i8);
    static_cast<void>(i9);
    static_cast<void>(i10);
    static_cast<void>(i11);
    static_cast<void>(i12);
    static_cast<void>(i13);
    static_cast<void>(i14);
    static_cast<void>(i15);
    static_cast<void>(i16);
    static_cast<void>(i17);
    static_cast<void>(i18);
    static_cast<void>(i19);
    static_cast<void>(i20);
    static_cast<void>(i21);
    static_cast<void>(i22);
    static_cast<void>(i23);
    static_cast<void>(i24);
    static_cast<void>(i25);
    static_cast<void>(i26);
    static_cast<void>(i27);
    static_cast<void>(i28);
    static_cast<void>(i29);
    static_cast<void>(i30);
    static_cast<void>(i31);
    static_cast<void>(i32);
    static_cast<void>(i33);
    static_cast<void>(i34);
    static_cast<void>(i35);
    static_cast<void>(i36);
    static_cast<void>(i37);
    static_cast<void>(i38);
    static_cast<void>(i39);
    static_cast<void>(i40);
    static_cast<void>(i41);
    static_cast<void>(i42);
    static_cast<void>(i43);
    static_cast<void>(i44);
    static_cast<void>(i45);
    static_cast<void>(i46);
    static_cast<void>(i47);
    static_cast<void>(i48);
    static_cast<void>(i49);
    static_cast<void>(i50);
    return f1 + f2 + f3 + f4 + f5 + f6 + f7 + f8 + f9 + f10 + f11 + f12 + f13 + f14 + f15 + f16 + f17 + f18 + f19 + f20 + f21 + f22 + f23 + f24 +
           f25 + f26 + f27 + f28 + f29 + f30 + f31 + f32 + f33 + f34 + f35 + f36 + f37 + f38 + f39 + f40 + f41 + f42 + f43 + f44 + f45 + f46 + f47 +
           f48 + f49 + f50;
  }

  static inline float sumMixedF(uint32_t i1, float f1, uint32_t i2, float f2, uint32_t i3, float f3, uint32_t i4, float f4, uint32_t i5, float f5,
                                uint32_t i6, float f6, uint32_t i7, float f7, uint32_t i8, float f8, uint32_t i9, float f9, uint32_t i10, float f10,
                                uint32_t i11, float f11, uint32_t i12, float f12, uint32_t i13, float f13, uint32_t i14, float f14, uint32_t i15,
                                float f15, uint32_t i16, float f16, uint32_t i17, float f17, uint32_t i18, float f18, uint32_t i19, float f19,
                                uint32_t i20, float f20, uint32_t i21, float f21, uint32_t i22, float f22, uint32_t i23, float f23, uint32_t i24,
                                float f24, uint32_t i25, float f25, uint32_t i26, float f26, uint32_t i27, float f27, uint32_t i28, float f28,
                                uint32_t i29, float f29, uint32_t i30, float f30, uint32_t i31, float f31, uint32_t i32, float f32, uint32_t i33,
                                float f33, uint32_t i34, float f34, uint32_t i35, float f35, uint32_t i36, float f36, uint32_t i37, float f37,
                                uint32_t i38, float f38, uint32_t i39, float f39, uint32_t i40, float f40, uint32_t i41, float f41, uint32_t i42,
                                float f42, uint32_t i43, float f43, uint32_t i44, float f44, uint32_t i45, float f45, uint32_t i46, float f46,
                                uint32_t i47, float f47, uint32_t i48, float f48, uint32_t i49, float f49, uint32_t i50, float f50,
                                void *const ctx) noexcept {
    static_cast<void>(ctx);
    static_cast<void>(i1);
    static_cast<void>(i2);
    static_cast<void>(i3);
    static_cast<void>(i4);
    static_cast<void>(i5);
    static_cast<void>(i6);
    static_cast<void>(i7);
    static_cast<void>(i8);
    static_cast<void>(i9);
    static_cast<void>(i10);
    static_cast<void>(i11);
    static_cast<void>(i12);
    static_cast<void>(i13);
    static_cast<void>(i14);
    static_cast<void>(i15);
    static_cast<void>(i16);
    static_cast<void>(i17);
    static_cast<void>(i18);
    static_cast<void>(i19);
    static_cast<void>(i20);
    static_cast<void>(i21);
    static_cast<void>(i22);
    static_cast<void>(i23);
    static_cast<void>(i24);
    static_cast<void>(i25);
    static_cast<void>(i26);
    static_cast<void>(i27);
    static_cast<void>(i28);
    static_cast<void>(i29);
    static_cast<void>(i30);
    static_cast<void>(i31);
    static_cast<void>(i32);
    static_cast<void>(i33);
    static_cast<void>(i34);
    static_cast<void>(i35);
    static_cast<void>(i36);
    static_cast<void>(i37);
    static_cast<void>(i38);
    static_cast<void>(i39);
    static_cast<void>(i40);
    static_cast<void>(i41);
    static_cast<void>(i42);
    static_cast<void>(i43);
    static_cast<void>(i44);
    static_cast<void>(i45);
    static_cast<void>(i46);
    static_cast<void>(i47);
    static_cast<void>(i48);
    static_cast<void>(i49);
    static_cast<void>(i50);
    return f1 + f2 + f3 + f4 + f5 + f6 + f7 + f8 + f9 + f10 + f11 + f12 + f13 + f14 + f15 + f16 + f17 + f18 + f19 + f20 + f21 + f22 + f23 + f24 +
           f25 + f26 + f27 + f28 + f29 + f30 + f31 + f32 + f33 + f34 + f35 + f36 + f37 + f38 + f39 + f40 + f41 + f42 + f43 + f44 + f45 + f46 + f47 +
           f48 + f49 + f50;
  }

  static inline float sumMixedFD_F(double d01, float f01, double d02, float f02, float f03, double d03, float f04, float f05, float f06, double d04,
                                   float f07, float f08, float f09, float f010, double d05, double d11, float f11, double d12, float f12, float f13,
                                   double d13, float f14, float f15, float f16, double d14, float f17, float f18, float f19, float f110, double d15,
                                   void *const ctx) noexcept {
    static_cast<void>(ctx);
    static_cast<void>(d01);
    static_cast<void>(d02);
    static_cast<void>(d03);
    static_cast<void>(d04);
    static_cast<void>(d05);
    static_cast<void>(d11);
    static_cast<void>(d12);
    static_cast<void>(d13);
    static_cast<void>(d14);
    static_cast<void>(d15);
    return f01 + f02 + f03 + f04 + f05 + f06 + f07 + f08 + f09 + f010 + f11 + f12 + f13 + f14 + f15 + f16 + f17 + f18 + f19 + f110;
  }

  static inline double sumMixedFD_D(double d01, float f01, double d02, float f02, float f03, double d03, float f04, float f05, float f06, double d04,
                                    float f07, float f08, float f09, float f10, double d05, double d11, float f11, double d12, float f12, float f13,
                                    double d13, float f14, float f15, float f16, double d14, float f17, float f18, float f19, float f20, double d15,
                                    void *const ctx) noexcept {
    static_cast<void>(ctx);
    static_cast<void>(f01);
    static_cast<void>(f02);
    static_cast<void>(f03);
    static_cast<void>(f04);
    static_cast<void>(f05);
    static_cast<void>(f06);
    static_cast<void>(f07);
    static_cast<void>(f08);
    static_cast<void>(f09);
    static_cast<void>(f10);
    static_cast<void>(f11);
    static_cast<void>(f12);
    static_cast<void>(f13);
    static_cast<void>(f14);
    static_cast<void>(f15);
    static_cast<void>(f16);
    static_cast<void>(f17);
    static_cast<void>(f18);
    static_cast<void>(f19);
    static_cast<void>(f20);
    return d01 + d02 + d03 + d04 + d05 + d11 + d12 + d13 + d14 + d15;
  }

  static inline float sumMixedDF_F(float f01, double d01, float f02, double d02, double d03, float f03, double d04, double d05, double d06, float f04,
                                   double d07, double d08, double d09, double d10, float f05, float f11, double d11, float f12, double d12,
                                   double d13, float f13, double d14, double d15, double d16, float f14, double d17, double d18, double d19,
                                   double d20, float f15, void *const ctx) noexcept {
    static_cast<void>(ctx);
    static_cast<void>(d01);
    static_cast<void>(d02);
    static_cast<void>(d03);
    static_cast<void>(d04);
    static_cast<void>(d05);
    static_cast<void>(d06);
    static_cast<void>(d07);
    static_cast<void>(d08);
    static_cast<void>(d09);
    static_cast<void>(d10);
    static_cast<void>(d11);
    static_cast<void>(d12);
    static_cast<void>(d13);
    static_cast<void>(d14);
    static_cast<void>(d15);
    static_cast<void>(d16);
    static_cast<void>(d17);
    static_cast<void>(d18);
    static_cast<void>(d19);
    static_cast<void>(d20);
    return f01 + f02 + f03 + f04 + f05 + f11 + f12 + f13 + f14 + f15;
  }

  static inline double sumMixedDF_D(float f01, double d01, float f02, double d02, double d03, float f03, double d04, double d05, double d06,
                                    float f04, double d07, double d08, double d09, double d010, float f05, float f11, double d11, float f12,
                                    double d12, double d13, float f13, double d14, double d15, double d16, float f14, double d17, double d18,
                                    double d19, double d110, float f15, void *const ctx) noexcept {
    static_cast<void>(ctx);
    static_cast<void>(f01);
    static_cast<void>(f02);
    static_cast<void>(f03);
    static_cast<void>(f04);
    static_cast<void>(f05);
    static_cast<void>(f11);
    static_cast<void>(f12);
    static_cast<void>(f13);
    static_cast<void>(f14);
    static_cast<void>(f15);
    return d01 + d02 + d03 + d04 + d05 + d06 + d07 + d08 + d09 + d010 + d11 + d12 + d13 + d14 + d15 + d16 + d17 + d18 + d19 + d110;
  }

  static inline uint32_t sumMixedIL_I(uint64_t l01, uint32_t i01, uint64_t l02, uint32_t i02, uint32_t i03, uint64_t l03, uint32_t i04, uint32_t i05,
                                      uint32_t i06, uint64_t l04, uint32_t i07, uint32_t i08, uint32_t i09, uint32_t i010, uint64_t l05, uint64_t l11,
                                      uint32_t i11, uint64_t l12, uint32_t i12, uint32_t i13, uint64_t l13, uint32_t i14, uint32_t i15, uint32_t i16,
                                      uint64_t l14, uint32_t i17, uint32_t i18, uint32_t i19, uint32_t i110, uint64_t l15, void *const ctx) noexcept {
    static_cast<void>(ctx);
    static_cast<void>(l01);
    static_cast<void>(l02);
    static_cast<void>(l03);
    static_cast<void>(l04);
    static_cast<void>(l05);
    static_cast<void>(l11);
    static_cast<void>(l12);
    static_cast<void>(l13);
    static_cast<void>(l14);
    static_cast<void>(l15);
    return i01 + i02 + i03 + i04 + i05 + i06 + i07 + i08 + i09 + i010 + i11 + i12 + i13 + i14 + i15 + i16 + i17 + i18 + i19 + i110;
  }

  static inline uint64_t sumMixedIL_L(uint64_t l01, uint32_t i01, uint64_t l02, uint32_t i02, uint32_t i03, uint64_t l03, uint32_t i04, uint32_t i05,
                                      uint32_t i06, uint64_t l04, uint32_t i07, uint32_t i08, uint32_t i09, uint32_t i10, uint64_t l05, uint64_t l11,
                                      uint32_t i11, uint64_t l12, uint32_t i12, uint32_t i13, uint64_t l13, uint32_t i14, uint32_t i15, uint32_t i16,
                                      uint64_t l14, uint32_t i17, uint32_t i18, uint32_t i19, uint32_t i20, uint64_t l15, void *const ctx) noexcept {
    static_cast<void>(ctx);
    static_cast<void>(i01);
    static_cast<void>(i02);
    static_cast<void>(i03);
    static_cast<void>(i04);
    static_cast<void>(i05);
    static_cast<void>(i06);
    static_cast<void>(i07);
    static_cast<void>(i08);
    static_cast<void>(i09);
    static_cast<void>(i10);
    static_cast<void>(i11);
    static_cast<void>(i12);
    static_cast<void>(i13);
    static_cast<void>(i14);
    static_cast<void>(i15);
    static_cast<void>(i16);
    static_cast<void>(i17);
    static_cast<void>(i18);
    static_cast<void>(i19);
    static_cast<void>(i20);
    return l01 + l02 + l03 + l04 + l05 + l11 + l12 + l13 + l14 + l15;
  }

  static inline uint32_t sumMixedLI_I(uint32_t i01, uint64_t l01, uint32_t i02, uint64_t l02, uint64_t l03, uint32_t i03, uint64_t l04, uint64_t l05,
                                      uint64_t l06, uint32_t i04, uint64_t l07, uint64_t l08, uint64_t l09, uint64_t l10, uint32_t i05, uint32_t i11,
                                      uint64_t l11, uint32_t i12, uint64_t l12, uint64_t l13, uint32_t i13, uint64_t l14, uint64_t l15, uint64_t l16,
                                      uint32_t i14, uint64_t l17, uint64_t l18, uint64_t l19, uint64_t l20, uint32_t i15, void *const ctx) noexcept {
    static_cast<void>(ctx);
    static_cast<void>(l01);
    static_cast<void>(l02);
    static_cast<void>(l03);
    static_cast<void>(l04);
    static_cast<void>(l05);
    static_cast<void>(l06);
    static_cast<void>(l07);
    static_cast<void>(l08);
    static_cast<void>(l09);
    static_cast<void>(l10);
    static_cast<void>(l11);
    static_cast<void>(l12);
    static_cast<void>(l13);
    static_cast<void>(l14);
    static_cast<void>(l15);
    static_cast<void>(l16);
    static_cast<void>(l17);
    static_cast<void>(l18);
    static_cast<void>(l19);
    static_cast<void>(l20);
    return i01 + i02 + i03 + i04 + i05 + i11 + i12 + i13 + i14 + i15;
  }

  static inline uint64_t sumMixedLI_L(uint32_t i01, uint64_t l01, uint32_t i02, uint64_t l02, uint64_t l03, uint32_t i03, uint64_t l04, uint64_t l05,
                                      uint64_t l06, uint32_t i04, uint64_t l07, uint64_t l08, uint64_t l09, uint64_t l010, uint32_t i05, uint32_t i11,
                                      uint64_t l11, uint32_t i12, uint64_t l12, uint64_t l13, uint32_t i13, uint64_t l14, uint64_t l15, uint64_t l16,
                                      uint32_t i14, uint64_t l17, uint64_t l18, uint64_t l19, uint64_t l110, uint32_t i15, void *const ctx) noexcept {
    static_cast<void>(ctx);
    static_cast<void>(i01);
    static_cast<void>(i02);
    static_cast<void>(i03);
    static_cast<void>(i04);
    static_cast<void>(i05);
    static_cast<void>(i11);
    static_cast<void>(i12);
    static_cast<void>(i13);
    static_cast<void>(i14);
    static_cast<void>(i15);
    return l01 + l02 + l03 + l04 + l05 + l06 + l07 + l08 + l09 + l010 + l11 + l12 + l13 + l14 + l15 + l16 + l17 + l18 + l19 + l110;
  }

  static inline void print(void *const ctx) noexcept {
    static_cast<void>(ctx);
  }
  static inline void print_i32(uint32_t value, void *const ctx) noexcept {
    static_cast<void>(ctx);
    static_cast<void>(value);
  }
  static inline void print_i64(uint64_t value, void *const ctx) noexcept {
    static_cast<void>(ctx);
    static_cast<void>(value);
  }
  static inline void print_f32(float value, void *const ctx) noexcept {
    static_cast<void>(ctx);
    static_cast<void>(value);
  }
  static inline void print_f64(double value, void *const ctx) noexcept {
    static_cast<void>(ctx);
    static_cast<void>(value);
  }
  static inline void print_i32_f32(uint32_t value1, float value2, void *const ctx) noexcept {
    static_cast<void>(ctx);
    static_cast<void>(value1);
    static_cast<void>(value2);
  }
  static inline void print_f64_f64(double value1, double value2, void *const ctx) noexcept {
    static_cast<void>(ctx);
    static_cast<void>(value1);
    static_cast<void>(value2);
  }

  static inline void func(void *const ctx) noexcept {
    static_cast<void>(ctx);
    assertStackAlignment();
  }
  static inline void func_i32(uint32_t value, void *const ctx) noexcept {
    static_cast<void>(ctx);
    assertStackAlignment();
    static_cast<void>(value);
  }
  static inline void func_f32(float value, void *const ctx) noexcept {
    static_cast<void>(ctx);
    assertStackAlignment();
    static_cast<void>(value);
  }
  static inline uint32_t func_ret_i32(void *const ctx) noexcept {
    static_cast<void>(ctx);
    assertStackAlignment();
    return 0;
  }
  static inline float func_ret_f32(void *const ctx) noexcept {
    static_cast<void>(ctx);
    assertStackAlignment();
    return 0;
  }
  static inline uint64_t func_i64_ret_i64(uint64_t value, void *const ctx) noexcept {
    static_cast<void>(ctx);
    assertStackAlignment();
    return value;
  }
  static inline uint32_t func_i32_ret_i32(uint32_t value, void *const ctx) noexcept {
    static_cast<void>(ctx);
    assertStackAlignment();
    return value;
  }

#if INTERRUPTION_REQUEST
  static inline void requestInterruption(void *const ctx) noexcept {
    vb::WasmModule *const wasmModule{vb::pCast<vb::WasmModule *>(ctx)};
    wasmModule->requestInterruption(vb::TrapCode::RUNTIME_INTERRUPT_REQUESTED);
  }
  static inline void requestInterruptionTrapCodeNone(void *const ctx) noexcept {
    vb::WasmModule *const wasmModule{vb::pCast<vb::WasmModule *>(ctx)};
    wasmModule->requestInterruption(vb::TrapCode::NONE);
  }
#endif

  static inline uint32_t getStacktraceCount(void *const ctx) noexcept {
    static_cast<void>(ctx);
    return static_cast<uint32_t>(lastStacktraceBuffer_->size());
  };
  static inline uint32_t getStacktraceEntry(uint32_t const index, void *const ctx) noexcept {
    static_cast<void>(ctx);
    if (index < getStacktraceCount(ctx)) {
      return (*lastStacktraceBuffer_)[index];
    }
    return 0xFFFF'FFFFU;
  };

  static inline uint32_t getU32FromLinearMemory(uint32_t const offset, void *const ctx) {
    vb::WasmModule const *const wasmModule{vb::pCast<vb::WasmModule *>(ctx)};
    const uint32_t res{vb::readFromPtr<uint32_t>(wasmModule->getLinearMemoryRegion(offset, 4U))};
    return res;
  }

  static inline uint32_t getU32FromLinearMemoryContextAtMem(uint32_t const offset, uint32_t const p1, uint32_t const p2, uint32_t const p3,
                                                            uint32_t const p4, uint32_t const p5, uint32_t const p6, uint32_t const p7,
                                                            uint32_t const p8, void *const ctx) {
    static_cast<void>(p1);
    static_cast<void>(p2);
    static_cast<void>(p3);
    static_cast<void>(p4);
    static_cast<void>(p5);
    static_cast<void>(p6);
    static_cast<void>(p7);
    static_cast<void>(p8);
    vb::WasmModule const *const wasmModule{vb::pCast<vb::WasmModule *>(ctx)};
    const uint32_t res{vb::readFromPtr<uint32_t>(wasmModule->getLinearMemoryRegion(offset, 4U))};
    return res;
  }

  static inline void setTraceBuffer(uint32_t const offset, uint32_t const size, void *const ctx) {
    vb::WasmModule *const wasmModule{vb::pCast<vb::WasmModule *>(ctx)};
    uint8_t const *const raw = wasmModule->getLinearMemoryRegion(offset, size * static_cast<uint32_t>(sizeof(uint32_t)));
    // Ensure 4-byte alignment (required for uint32_t*)
    uintptr_t const addr = vb::pToNum(raw);
    if ((addr % alignof(uint32_t)) != 0) {
      std::cout << "Trace buffer base address not uint32_t-aligned (address=0x" << std::hex << addr << ", offset=" << std::dec << offset << ")"
                << std::endl;
      std::terminate();
    }

    uint32_t *const ptr = vb::numToP<uint32_t *>(addr);
    wasmModule->setTraceBuffer(vb::Span<uint32_t>{ptr, size});
  }

  class MultiReturn final
      : public ImportFunctionV2<std::tuple<uint32_t, uint64_t, uint32_t, double, float>, std::tuple<uint32_t, uint64_t, uint32_t, double, float>> {
  public:
    using ImportFunctionV2::ImportFunctionV2;
    static void call(void *params, void *results, void *ctx) {
      uint32_t const p0 = getParam<0>(params);
      uint64_t const p1 = getParam<1>(params);
      uint32_t const p2 = getParam<2>(params);
      double const p3 = getParam<3>(params);
      float const p4 = getParam<4>(params);
      static_cast<void>(ctx);
      setRet<0>(results, p0 + 1);
      setRet<1>(results, p1 + 2);
      setRet<2>(results, p2 + 3);
      setRet<3>(results, p3 + 4.4);
      setRet<4>(results, p4 + 5.5F);
    }
  };

  static auto makeImports() {
    return vb::make_array(
        MultiReturn::generateNativeSymbol("spectest", "multiReturn", vb::NativeSymbol::Linkage::DYNAMIC, MultiReturn::call),
        //
        DYNAMIC_LINK("spectest", "setTraceBuffer", setTraceBuffer),
        //
        DYNAMIC_LINK("spectest", "nop", nop), DYNAMIC_LINK("spectest", "func-i64-i64", func_i64_i64),
        //
        DYNAMIC_LINK("test", "func", func), DYNAMIC_LINK("test", "func-i32", func_i32), DYNAMIC_LINK("test", "func-f32", func_f32),
        DYNAMIC_LINK("test", "func->i32", func_ret_i32), DYNAMIC_LINK("test", "func->f32", func_ret_f32),
        DYNAMIC_LINK("test", "func-i32->i32", func_i32_ret_i32), DYNAMIC_LINK("test", "func-i64->i64", func_i64_ret_i64),
        //
        DYNAMIC_LINK("spectest", "print", print), DYNAMIC_LINK("spectest", "print_i32", print_i32), DYNAMIC_LINK("spectest", "print_i64", print_i64),
        DYNAMIC_LINK("spectest", "print_f32", print_f32), DYNAMIC_LINK("spectest", "print_f64", print_f64),
        DYNAMIC_LINK("spectest", "print_i32_f32", print_i32_f32), DYNAMIC_LINK("spectest", "print_f64_f64", print_f64_f64),
        //
        DYNAMIC_LINK("spectest", "sumI", sumI), DYNAMIC_LINK("spectest", "sumF", sumF), DYNAMIC_LINK("spectest", "sumLastI", sumLastI),
        DYNAMIC_LINK("spectest", "sumLastF", sumLastF), DYNAMIC_LINK("spectest", "sumMixedI", sumMixedI),
        DYNAMIC_LINK("spectest", "sumMixedF", sumMixedF), DYNAMIC_LINK("spectest", "sumMixedFD_F", sumMixedFD_F),
        DYNAMIC_LINK("spectest", "sumMixedFD_D", sumMixedFD_D), DYNAMIC_LINK("spectest", "sumMixedDF_F", sumMixedDF_F),
        DYNAMIC_LINK("spectest", "sumMixedDF_D", sumMixedDF_D), DYNAMIC_LINK("spectest", "sumMixedIL_I", sumMixedIL_I),
        DYNAMIC_LINK("spectest", "sumMixedIL_L", sumMixedIL_L), DYNAMIC_LINK("spectest", "sumMixedLI_I", sumMixedLI_I),
        DYNAMIC_LINK("spectest", "sumMixedLI_L", sumMixedLI_L),
#if INTERRUPTION_REQUEST
        DYNAMIC_LINK("spectest", "requestInterruption", requestInterruption),
        DYNAMIC_LINK("spectest", "requestInterruptionTrapCodeNone", requestInterruptionTrapCodeNone),
#endif
        //
        DYNAMIC_LINK("spectest", "getU32FromLinearMemory", getU32FromLinearMemory),
        DYNAMIC_LINK("spectest", "getU32FromLinearMemoryContextAtMem", getU32FromLinearMemoryContextAtMem),
        //
        DYNAMIC_LINK("spectest", "getStacktraceCount", getStacktraceCount), DYNAMIC_LINK("spectest", "getStacktraceEntry", getStacktraceEntry));
  }

  static inline void setLastStacktraceBuffer(std::vector<uint32_t> *const lastStacktraceBuffer) noexcept {
    lastStacktraceBuffer_ = lastStacktraceBuffer;
  }

  static inline void assertStackAlignment() noexcept {
#if (defined __GNUC__) || (defined __clang__)
    // Check if current sp is aligned to 16
    void const *const frameAddress = __builtin_frame_address(0);
    uintptr_t const addressNum = vb::pToNum(frameAddress);

#if defined(JIT_TARGET_TRICORE)
    if ((addressNum % 8U) != 0) {
      std::cout << "address is not aligned to 8: " << std::hex << addressNum << std::endl;
      std::terminate();
    }
#else
    if ((addressNum % 16U) != 0) {
      std::cout << "address is not aligned to 16: " << std::hex << addressNum << std::endl;
      std::terminate();
    }
#endif // JIT_TARGET_TRICORE

#endif
  }

private:
#ifdef JIT_TARGET_TRICORE
  static std::vector<uint32_t> *lastStacktraceBuffer_;
#else
  thread_local static std::vector<uint32_t> *lastStacktraceBuffer_;
#endif
};

} // namespace spectest

#endif

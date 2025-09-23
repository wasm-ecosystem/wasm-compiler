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

#include <pybind11/pybind11.h>

#include "binding/python/binding.hpp"

#include "src/core/common/WasmType.hpp"
#include "src/core/compiler/common/ManagedBinary.hpp"

namespace vb {

void binding::memoryFnc(ExtendableMemory &currentObject, uint32_t minimumLength, void *const ctx) {
  static_cast<void>(ctx);
  if (minimumLength == 0) {
    free(currentObject.data());
  } else {
    minimumLength = std::max(minimumLength, static_cast<uint32_t>(1000U)) * 2U;
    currentObject.reset(vb::pCast<uint8_t *>(realloc(currentObject.data(), minimumLength)), minimumLength);
  }
}

void *binding::allocFnc(uint32_t size, void *ctx) {
  static_cast<void>(ctx);
  return malloc(static_cast<size_t>(size));
}

void binding::freeFnc(void *ptr, void *ctx) {
  static_cast<void>(ctx);
  free(ptr);
}

} // namespace vb

#ifndef VB_BINDING_NAME
#define VB_BINDING_NAME vb_warp
#endif

PYBIND11_MODULE(VB_BINDING_NAME, m) {
  pybind11::enum_<vb::WasmType>(m, "WasmType", pybind11::module_local())
      .value("TVoid", vb::WasmType::TVOID)
      .value("I32", vb::WasmType::I32)
      .value("I64", vb::WasmType::I64)
      .value("F32", vb::WasmType::F32)
      .value("F64", vb::WasmType::F64);

  pybind11::class_<vb::ManagedBinary>(m, "ManagedBinary", pybind11::module_local()); // NOLINT

  vb::binding::bindingCompiler(m);
  vb::binding::bindingRuntime(m);
}

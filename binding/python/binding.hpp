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

#ifndef BINDING_PYTHON_BINDING_HPP
#define BINDING_PYTHON_BINDING_HPP

#include <pybind11/pybind11.h>

#include "src/core/common/ExtendableMemory.hpp"

namespace vb {
namespace binding {

extern void bindingCompiler(pybind11::module_ &m);
extern void bindingRuntime(pybind11::module_ &m);

void memoryFnc(ExtendableMemory &currentObject, uint32_t minimumLength, void *const ctx);
void *allocFnc(uint32_t size, void *ctx);
void freeFnc(void *ptr, void *ctx);

} // namespace binding
} // namespace vb

#endif // BINDING_PYTHON_BINDING_HPP

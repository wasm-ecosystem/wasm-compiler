///
/// @file Extensions.cpp
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
#include "extensions/Tracing.hpp"

#include "src/core/runtime/Runtime.hpp"
#include "src/extensions/Extension.hpp"

void vb::extension::registerRuntime(vb::Runtime &runtime) {
  traceExtension.registerRuntime(runtime);
}

void vb::extension::unregisterRuntime(vb::Runtime &runtime) {
  traceExtension.unregisterRuntime(runtime);
}

void vb::extension::stop() {
  traceExtension.stopAndWriteData();
}

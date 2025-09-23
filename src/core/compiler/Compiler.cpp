///
/// @file Compiler.cpp
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
#include <cstdint>
#include <utility>

#include "Compiler.hpp"

#include "src/config.hpp"
#include "src/core/common/ExtendableMemory.hpp"
#include "src/core/common/NativeSymbol.hpp"
#include "src/core/common/Span.hpp"
#include "src/core/compiler/backend/aarch64/aarch64_backend.hpp"
#include "src/core/compiler/backend/tricore/tricore_backend.hpp"
#include "src/core/compiler/backend/x86_64/x86_64_backend.hpp"
#include "src/core/compiler/common/BumpAllocator.hpp"
#include "src/core/compiler/common/Common.hpp"
#include "src/core/compiler/common/ManagedBinary.hpp"
#include "src/core/compiler/common/MemWriter.hpp"
#include "src/core/compiler/common/ModuleInfo.hpp"
#include "src/core/compiler/common/Stack.hpp"
#include "src/core/compiler/frontend/Frontend.hpp"
#include "src/core/compiler/frontend/ValidationStack.hpp"

namespace vb {

Compiler::Compiler(ReallocFnc const compilerMemoryReallocFnc, AllocFnc const compilerMemoryAllocFnc, FreeFnc const compilerMemoryFreeFnc,
                   void *const ctx, ReallocFnc const binaryMemoryReallocFnc, bool const allowUnknownImports)
    : stack_(Stack(compilerMemoryAllocFnc, compilerMemoryFreeFnc, ctx)),
      validationStack_(ValidationStack(moduleInfo_, compilerMemoryAllocFnc, compilerMemoryFreeFnc, ctx)),
      memory_(ExtendableMemory(compilerMemoryReallocFnc)), output_(ExtendableMemory(binaryMemoryReallocFnc)),
      backend_(stack_, moduleInfo_, memory_, output_, common_, *this), logger_(nullptr), debugMode_(false),
      forceHighRegisterPressureForTesting_(false), stacktraceRecordCount_(0U), allowUnknownImports_(allowUnknownImports), common_(Common(*this))
#if ENABLE_EXTENSIONS
      ,
      dwarfGenerator_(nullptr), analytics_(nullptr)
#endif
{
}

Compiler::Compiler(ReallocFnc const compilerMemoryReallocFnc, AllocFnc const compilerMemoryAllocFnc, FreeFnc const compilerMemoryFreeFnc,
                   void *const ctx, ExtendableMemory &&binaryMemory, bool const allowUnknownImports)
    : stack_(Stack(compilerMemoryAllocFnc, compilerMemoryFreeFnc, ctx)),
      validationStack_(ValidationStack(moduleInfo_, compilerMemoryAllocFnc, compilerMemoryFreeFnc, ctx)),
      memory_(ExtendableMemory(compilerMemoryReallocFnc)), output_(std::move(binaryMemory)),
      backend_(stack_, moduleInfo_, memory_, output_, common_, *this), logger_(nullptr), debugMode_(false),
      forceHighRegisterPressureForTesting_(false), stacktraceRecordCount_(0U), allowUnknownImports_(allowUnknownImports), common_(Common(*this))
#if ENABLE_EXTENSIONS
      ,
      dwarfGenerator_(nullptr), analytics_(nullptr)
#endif
{
} // namespace vb

ManagedBinary Compiler::compile(Span<uint8_t const> const &bytecode, Span<NativeSymbol const> const &symbolList) {
  Frontend frontend{bytecode, symbolList, moduleInfo_, stack_, memory_, common_, *this, validationStack_};
  frontend.startCompilation(forceHighRegisterPressureForTesting_);

  ManagedBinary outputBinary{output_.toManagedBinary()};
  output_.flush();

#if ENABLE_EXTENSIONS
  if (analytics_ != nullptr) {
    analytics_->setBinarySizes(static_cast<uint32_t>(bytecode.size()), outputBinary.size());
  }
#endif

  return outputBinary;
}

void Compiler::forceHighRegisterPressureForTesting() VB_NOEXCEPT {
  forceHighRegisterPressureForTesting_ = true;
}

void Compiler::enableDebugMode(ReallocFnc const debugMapReallocFnc) VB_NOEXCEPT {
  debugMode_ = true;
  debugMap_ = MemWriter(ExtendableMemory(debugMapReallocFnc));
}

void Compiler::disableDebugMode() VB_NOEXCEPT {
  debugMode_ = false;
  debugMap_ = MemWriter(ExtendableMemory());
}

} // namespace vb

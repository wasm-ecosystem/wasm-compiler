///
/// @file AssemblerAdapter.hpp
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
#ifndef ASSEMBLER_ADAPTER_HPP
#define ASSEMBLER_ADAPTER_HPP

#include "src/config.hpp"

namespace vb {
namespace x86_64 {
class x86_64Assembler;
}
namespace aarch64 {
class AArch64_Assembler;
}
namespace tc {
class Tricore_Assembler;
}

#ifdef JIT_TARGET_X86_64
using TAssembler = x86_64::x86_64Assembler; ///< Assembler class (x86_64)
#elif defined JIT_TARGET_AARCH64
using TAssembler = aarch64::AArch64_Assembler; ///< Assembler class (AArch64)
#elif defined JIT_TARGET_TRICORE
using TAssembler = tc::Tricore_Assembler; ///< Assembler class (Tricore)
#else
static_assert(false, "Backend not supported");
#endif
} // namespace vb

#endif

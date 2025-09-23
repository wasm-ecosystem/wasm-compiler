///
/// @file RegAdapter.hpp
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
#ifndef REGADAPTER_HPP
#define REGADAPTER_HPP

#include "src/config.hpp"

#if defined(JIT_TARGET_X86_64)
#include "x86_64/x86_64_cc.hpp"
#include "x86_64/x86_64_encoding.hpp"
namespace vb {
using TReg = x86_64::REG; ///< Register type (x86_64)
namespace NBackend = x86_64;
} // namespace vb
#elif defined(JIT_TARGET_AARCH64)
#include "aarch64/aarch64_cc.hpp"
#include "aarch64/aarch64_encoding.hpp"
namespace vb {
using TReg = aarch64::REG; ///< Register type (AArch64)
namespace NBackend = aarch64;
} // namespace vb
#elif defined(JIT_TARGET_TRICORE)
#include "tricore/tricore_cc.hpp"
#include "tricore/tricore_encoding.hpp"
namespace vb {
using TReg = tc::REG; ///< Register type (Tricore)
namespace NBackend = tc;
} // namespace vb
#else
static_assert(false, "Backend not supported");
#endif

#endif

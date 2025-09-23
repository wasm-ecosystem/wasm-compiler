///
/// @file windows_clean.hpp
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
#ifndef WINDOWS_CLEAN_H
#if defined(_WIN32) || defined(__CYGWIN__)

#ifdef __MINGW32__
#if _WIN32_WINNT != 0x0A00
#undef _WIN32_WINNT
#define _WIN32_WINNT 0x0A00
#endif
#endif

// clang-format off
#include <windows.h>
#include <memoryapi.h>
#include <malloc.h>
#include "wintt_undef.hpp"
// clang-format on
#endif
#endif

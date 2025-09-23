///
/// @file SanitizeHelper.hpp
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
#ifndef SANITIZE_HELPER_HPP
#define SANITIZE_HELPER_HPP

#if defined(__has_feature)
#if (__has_feature(thread_sanitizer))
#define COMPILER_SUPPORTS_THREAD_SANITIZER
#endif
#endif

#if !(defined COMPILER_SUPPORTS_THREAD_SANITIZER) && (defined __SANITIZE_THREAD__)
#define COMPILER_SUPPORTS_THREAD_SANITIZER
#endif

#ifdef COMPILER_SUPPORTS_THREAD_SANITIZER
#define NO_THREAD_SANITIZE __attribute__((no_sanitize("thread")))
#else
#define NO_THREAD_SANITIZE
#endif

#endif

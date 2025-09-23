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

#ifndef DBG_FUZZ_HPP
#define DBG_FUZZ_HPP

#define GDB_FUZZ_INPUT_BINARY_INIT(MAXLENGTH)                                                                                                        \
  extern "C" {                                                                                                                                       \
  volatile uint8_t VBHELPER_GDB_FUZZ_INPUT_BINARY[MAXLENGTH];                                                                                        \
  volatile uint32_t VBHELPER_GDB_FUZZ_INPUT_BINARY_ACTUAL_LENGTH = 0U;                                                                               \
  volatile uint32_t VBHELPER_GDB_FUZZ_INPUT_REFOUTPUT_LENGTH = 0U;                                                                                   \
  volatile bool VBHELPER_GDB_FUZZ_ITERATION_FAILED = false;                                                                                          \
  volatile bool VBHELPER_INPUT_IS_ALREADY_COMPILED = false;                                                                                          \
  }

#define GDB_FUZZ_INPUT_BINARY &VBHELPER_GDB_FUZZ_INPUT_BINARY[0]

#define GDB_FUZZ_INPUT_BINARY_ACTUAL_LENGTH VBHELPER_GDB_FUZZ_INPUT_BINARY_ACTUAL_LENGTH
#define GDB_FUZZ_INPUT_REFOUTPUT_LENGTH VBHELPER_GDB_FUZZ_INPUT_REFOUTPUT_LENGTH
#define GDB_FUZZ_ITERATION_FAILED VBHELPER_GDB_FUZZ_ITERATION_FAILED

#define GDB_FUZZ_OUTPUT(MAXLENGTH)                                                                                                                   \
  extern "C" {                                                                                                                                       \
  uint8_t VBHELPER_GDB_FUZZ_OUTPUT_RESULT[MAXLENGTH];                                                                                                \
  uint32_t VBHELPER_GDB_FUZZ_OUTPUT_RESULT_LENGTH = 0U;                                                                                              \
  uint8_t VBHELPER_GDB_FUZZ_OUTPUT_MESSAGE[MAXLENGTH];                                                                                               \
  uint32_t VBHELPER_GDB_FUZZ_OUTPUT_MESSAGE_SIZE = 0;                                                                                                \
  }

#endif

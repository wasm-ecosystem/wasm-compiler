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

#pragma once

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <string>

inline std::string wasmFeatureFlags() {
  char const *const profile{std::getenv("VB_FUZZ_WASM_FEATURE_PROFILE")};
  std::ifstream profileFile{"fuzz/wasm_feature_profiles.json"};
  if (!profileFile) {
    std::cerr << "Unable to read fuzz/wasm_feature_profiles.json" << std::endl;
    std::exit(-1);
  }

  nlohmann::json profiles{};
  profileFile >> profiles;

  std::string const profileName{profile == nullptr ? "release" : profile};
  auto const profileIt{profiles["profiles"].find(profileName)};
  if (profileIt == profiles["profiles"].end()) {
    std::cerr << "Unsupported WebAssembly feature profile: " << profileName << std::endl;
    std::exit(-1);
  }

  std::string flags{};
  for (nlohmann::json const &flag : profileIt->at("binaryen_flags")) {
    flags += " " + flag.get<std::string>();
  }
  return flags;
}

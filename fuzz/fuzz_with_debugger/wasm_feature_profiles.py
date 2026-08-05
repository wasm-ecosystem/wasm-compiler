# Copyright (C) 2025 Bayerische Motoren Werke Aktiengesellschaft (BMW AG)
# SPDX-License-Identifier: Apache-2.0
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

import json
import os
from pathlib import Path


PROFILE_FILE = Path(__file__).resolve().parents[1] / "wasm_feature_profiles.json"


def wasm_feature_flags() -> list[str]:
    profile = os.getenv("VB_FUZZ_WASM_FEATURE_PROFILE", "release")
    with PROFILE_FILE.open(encoding="utf-8") as profile_file:
        profiles = json.load(profile_file)["profiles"]

    if profile not in profiles:
        raise ValueError(f"Unsupported WebAssembly feature profile: {profile}")
    return profiles[profile]["binaryen_flags"]

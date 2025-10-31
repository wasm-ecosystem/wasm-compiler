#! /usr/bin/env bash
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


echo "start archive" &&
    git submodule deinit --all &&
    git submodule update --init --depth=1 thirdparty/berkeley-softfloat-3 &&
    # Build a temporary file list for tar so we can exclude wasm_examples and add the SPDX file
    tmpfile=$(mktemp)
    # Write tracked files excluding wasm_examples into the temp file
    git ls-files --recurse-submodules | grep -v '^wasm_examples/' > "$tmpfile"
    # Ensure SPDX files are included if they exist (they may be generated earlier in the workflow)
    if [ -f wasm-compiler.spdx3.jsonld.json ]; then
        echo "wasm-compiler.spdx3.jsonld.json" >> "$tmpfile"
    fi
    # Create package name if not provided
    outname=$([[ -z $package_name ]] && echo wasm-compiler-$(git describe --tags --exact-match HEAD || git rev-parse HEAD).tar.gz || echo $package_name)
    tar caf "$outname" -T "$tmpfile"
    rm -f "$tmpfile"

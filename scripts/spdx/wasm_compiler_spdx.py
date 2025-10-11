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

from BerkeleySoftFloatSPDX import BerkeleySoftFloatSPDX
from WasmCompilerSPDX import WasmCompilerSPDX
import os
import argparse

if __name__ == "__main__":
    parser = argparse.ArgumentParser(
        description="Generate SPDX files for wasm-compiler and its dependencies."
    )
    parser.add_argument(
        "-o", "--output_dir", type=str, help="output spdx file dir", default=os.getcwd()
    )

    args = parser.parse_args()

    print("Generating consolidated SPDX for wasm-compiler and dependencies...")
    wasm_compiler_spdx_creator = WasmCompilerSPDX(args.output_dir)
    wasm_compiler_spdx_creator.create_spdx_file()

    print("SPDX generation completed!")

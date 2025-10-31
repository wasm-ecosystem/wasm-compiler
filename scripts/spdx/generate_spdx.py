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
from spdx_tools.spdx.validation.document_validator import validate_full_spdx_document
from spdx_tools.spdx.parser.parse_anything import parse_file

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

    # Validate the generated file
    print("\n" + "=" * 60)
    print("Validating generated SPDX file...")
    print("=" * 60)
    spdx_file = os.path.join(args.output_dir, "wasm-compiler.spdx")

    try:
        document = parse_file(spdx_file)
        validation_messages = validate_full_spdx_document(document)

        if validation_messages:
            print(f"Validation failed with {len(validation_messages)} errors:")
            for msg in validation_messages:
                print(f"  - {msg.validation_message}")
            exit(1)
        else:
            print("SPDX file is valid!")
            print(f"   Location: {spdx_file}")
    except Exception as e:
        print(f"Validation error: {e}")
        import traceback

        traceback.print_exc()
        exit(1)

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

# - name: codegen test
#   shell: bash
#   run: python3 scripts/code_gen_test.py --no-color
# - name: debug info test
#   shell: bash
#   run: python3 scripts/debug_info_gen_test.py --no-color
import os
import argparse

parser = argparse.ArgumentParser()
parser.add_argument(
    "--no-color", action="store_false", dest="color", help="Disable color output"
)

args = parser.parse_args()

has_color = "" if args.color else " --no-color"

if os.system(f"python3 scripts/code_gen_test.py {has_color}") != 0:
    print("Code generation test failed.")
    exit(1)
if os.system(f"python3 scripts/debug_info_gen_test.py {has_color}") != 0:
    print("Debug info generation test failed.")
    exit(1)

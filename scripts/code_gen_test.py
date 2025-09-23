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

# FileCheck usage: https://llvm.org/docs/CommandGuide/FileCheck.html

import os
import sys
from typing import List, Dict, Tuple
import argparse
from helper import dis, wat_parser, wasm_utils, file_check

workspace = os.path.abspath(os.path.join(__file__, "..", ".."))

test_suites = os.path.join(workspace, "tests", "codegen")

import aarch64_vb_warp
import aarch64_active_vb_warp
import x86_64_vb_warp
import x86_64_active_vb_warp
import tricore_vb_warp

targets = {
    "aarch64": aarch64_vb_warp,
    "aarch64_active": aarch64_active_vb_warp,
    "x86_64": x86_64_vb_warp,
    "x86_64_active": x86_64_active_vb_warp,
    "tricore": tricore_vb_warp,
}

for _, module in targets.items():
    module.enable_color(False)


def collect_cases(root: str) -> List[str]:
    files = []
    for path in os.listdir(root):
        if path.endswith(".wat"):
            fullpath = os.path.join(root, path)
            files.append(fullpath)
    return files


def run(case_path: str, args) -> bool:
    wat = open(case_path).read()

    for target_name, module in targets.items():
        if args.backend is not None and target_name != args.backend:
            continue

        compiler = module.Compiler()
        wasm_binary = wasm_utils.wat_to_wasm(path=case_path)
        dis_lines = compiler.disassemble_wasm(wasm_binary)
        config = dis.parse_config_str(module.get_configuration())
        file_check_prefix = file_check.convert_config_to_file_check_prefix(config)
        dis_output, _ = dis.process_dis_output(
            dis_lines=dis_lines, config=config, has_memory=wat_parser.has_memory(wat)
        )

        is_success, _, stderr = file_check.check_file(
            case_path, dis_output, file_check_prefix, args.color
        )
        if not is_success:
            print(config)
            print("====================== START DIS ======================")
            print(dis_output)
            print("======================  END  DIS ======================")
            print()
            print(file_check_prefix)
            print("==================== START PATTERN ====================")
            print(stderr)
            print("====================  END  PATTERN ====================")
            return False
    return True


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--color", action="store_true", default=True, help="Enable color output"
    )
    parser.add_argument(
        "--no-color", action="store_false", dest="color", help="Disable color output"
    )
    parser.add_argument("--case", default=None, help="explicit defined test case")
    parser.add_argument("--backend", default=None, help="Special target")

    args = parser.parse_args()

    assert args.backend is None or args.backend in targets

    failed_case_count = 0
    cases_path = collect_cases(root=test_suites)
    for case_path in cases_path:
        if args.case is not None and case_path != os.path.abspath(args.case):
            continue
        print(f"run {case_path}")
        is_success = run(case_path=case_path, args=args)
        if not is_success:
            failed_case_count += 1
    if failed_case_count > 0:
        sys.exit(-1)

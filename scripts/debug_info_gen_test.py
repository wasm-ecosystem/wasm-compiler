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

from math import exp
import os
import sys
from typing import List, Dict
import argparse
from helper import dis, wat_parser, wasm_utils, dwarf, file_check

workspace = os.path.abspath(os.path.join(__file__, "..", ".."))

test_suites = os.path.join(workspace, "tests", "debug_info")

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


def get_dwo_file_check_prefix(file_check_prefix: str) -> str:
    return "DWO_" + file_check_prefix


class DebugLineMapping:
    def __init__(self, wat_line_index: int, assembly_line: str):
        self.wat_line_index = wat_line_index
        self.assembly_line = assembly_line

    def to_string(self):
        # FileCheck requires 1-based line numbers
        return f"{self.wat_line_index+1}: {self.assembly_line}"


def get_output(module, wat_str: str, config: Dict[str, str]):
    wasm = wasm_utils.wat_to_wasm(wat=wat_str.encode("utf-8"))
    compiler = module.Compiler()
    compiler.set_stacktrace_record_count(1)
    compiler.enable_dwarf(True)
    dis_lines = compiler.disassemble_wasm(wasm)
    dwo: bytes = compiler.get_dwarf_object()
    assembly, _ = dis.process_dis_output(
        dis_lines=dis_lines, config=config, has_memory=wat_parser.has_memory(wat_str)
    )
    wat_line_to_assembly_line = dwarf.analyze_debug_info_in_dwarf(
        dwo=dwo,
        wasm=wasm,
        assembly=assembly,
    )
    wat_line_to_assembly_line = list(wat_line_to_assembly_line.items())
    # ordered by wat line number
    wat_line_to_assembly_line.sort(key=lambda x: x[0])
    assembly_list = assembly.split("\n")
    results: List[DebugLineMapping] = []
    for wat_line_index, assembly_line_indexes in wat_line_to_assembly_line:
        for assembly_line_index in assembly_line_indexes:
            assembly_line = assembly_list[assembly_line_index]
            first_space = assembly_line.find(" ")
            assert first_space != -1, "Assembly line must have a space"
            assembly_line_without_offset_prefix = assembly_line[
                first_space + 1 :
            ].strip()
            results.append(
                DebugLineMapping(wat_line_index, assembly_line_without_offset_prefix)
            )
    dwo_dump: str = dwarf.dump_dwo(dwo)
    return (results, dwo_dump)


def collect_cases(root: str) -> List[str]:
    files = []
    for path in os.listdir(root):
        if path.endswith(".wat"):
            fullpath = os.path.join(root, path)
            files.append(fullpath)
    return files


def get_check_prefix(base_prefix: str, is_next: bool) -> str:
    return (f"{base_prefix}-NEXT:" if is_next else f"{base_prefix}:").ljust(24)


def run(case_path: str, args) -> bool:
    wat_str = open(case_path, "r").read()
    prefix_map: Dict[str, str] = {}
    expected_debug_info_map: Dict[str, List[DebugLineMapping]] = {}
    expected_dwo_dump_map: Dict[str, str] = {}
    for target_name, module in targets.items():
        if args.backend is not None and target_name != args.backend:
            continue
        config = dis.parse_config_str(module.get_configuration())
        expected_debug_info, dwo_dump = get_output(module, wat_str, config)
        if args.update:
            short_prefix = file_check.convert_config_to_file_check_short_prefix(config)
            assert short_prefix != None
            print(f"updating {short_prefix}")
            expected_debug_info_map[target_name] = expected_debug_info
            expected_dwo_dump_map[target_name] = dwo_dump
            prefix_map[target_name] = short_prefix
        else:
            file_check_prefix = file_check.convert_config_to_file_check_short_prefix(
                config
            )
            is_success, _, stderr = file_check.check_file(
                case_path,
                "\n".join([info.to_string() for info in expected_debug_info]),
                [file_check_prefix],
                args.color,
            )
            if not is_success:
                print(config)
                print(file_check_prefix)
                print("==================== START PATTERN ====================")
                print(stderr)
                print("====================  END  PATTERN ====================")
                return False
            dwo_file_check_prefix = get_dwo_file_check_prefix(file_check_prefix)
            is_success, _, stderr = file_check.check_file(
                case_path,
                dwo_dump,
                [dwo_file_check_prefix],
                args.color,
            )
            if not is_success:
                print(config)
                print(dwo_file_check_prefix)
                print("==================== START PATTERN ====================")
                print(stderr)
                print("====================  END  PATTERN ====================")
                return False
    if args.update:
        wat_lines = wat_str.split("\n")
        new_wat_lines = [";; auto-generated by scripts/debug_info_gen_test.py --update"]
        debug_info_indexes: Dict[str, int] = {}
        for target_name, _ in expected_debug_info_map.items():
            debug_info_indexes[target_name] = 0

        def skip(current: int) -> int:
            next = current
            while next < len(wat_lines) and (
                wat_lines[next].startswith(";;") or len(wat_lines[next]) == 0
            ):
                next += 1
            return next

        wat_line_index = skip(0)
        while wat_line_index < len(wat_lines):
            wat_line = wat_lines[wat_line_index]
            new_wat_lines.append(wat_line)
            file_check_line_offset = 0
            for target_name, expected_debug_info in expected_debug_info_map.items():
                debug_info_index = debug_info_indexes[target_name]
                # emit all related instruction after corresponded wat line
                while (
                    debug_info_index < len(expected_debug_info)
                    and expected_debug_info[debug_info_index].wat_line_index
                    == wat_line_index
                ):
                    file_check_line_offset += 1
                    file_check_prefix = get_check_prefix(
                        prefix_map[target_name], debug_info_index != 0
                    )
                    new_wat_lines.append(
                        f";; {file_check_prefix} [[@LINE-{file_check_line_offset}]]: {expected_debug_info[debug_info_index].assembly_line}"
                    )
                    debug_info_index += 1
                debug_info_indexes[target_name] = debug_info_index
            wat_line_index = skip(wat_line_index + 1)

            new_wat_lines.append("")

        for target_name, expected_dwo_dump in expected_dwo_dump_map.items():
            prefix = get_dwo_file_check_prefix(prefix_map[target_name])
            expected_dwo_dump_lines = expected_dwo_dump.split("\n")
            for i in range(len(expected_dwo_dump_lines)):
                l = expected_dwo_dump_lines[i]
                if len(l) == 0:
                    new_wat_lines.append(f";; {prefix}-EMPTY:")
                else:
                    file_check_prefix = get_check_prefix(prefix, i != 0)
                    new_wat_lines.append(f";; {file_check_prefix} {l}")
            new_wat_lines.append("")

        open(case_path, "w").write("\n".join(new_wat_lines))
    return True


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--no-color", action="store_false", dest="color", help="Disable color output"
    )
    parser.add_argument("--case", default=None, help="Explicit defined test case")
    parser.add_argument("--backend", default=None, help="Special target")
    parser.add_argument(
        "--update",
        action="store_true",
        default=False,
        help="Update the test case with the latest output",
    )

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

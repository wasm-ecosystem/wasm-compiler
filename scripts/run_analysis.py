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

import argparse
from io import TextIOWrapper
import math
import os
import sys
from statistics import geometric_mean
import importlib
from helper import dwarf, wasm_parser
from collections import defaultdict
from typing import Dict

vb_targets = [
    "aarch64_vb_warp",
    "aarch64_active_vb_warp",
    "x86_64_vb_warp",
    "x86_64_active_vb_warp",
    "tricore_vb_warp",
]


parser = argparse.ArgumentParser(
    description="Helper script to automatically run jit code benchmarks for wasm-compiler for CI"
)
parser.add_argument(
    "-i", "--input", help="Path to the wasm-compiler-benchmark folder", required=True
)
parser.add_argument("-o", "--output", help="Path to the report")
parser.add_argument(
    "--detail", help="Path to the detail report, will output in markdown format"
)
parser.add_argument(
    "--cost-model-prefix",
    help="Path to the cost model report, will output in plain text",
)

args = parser.parse_args()

output_file = open(args.output, "w") if args.output else sys.stdout
detail_file = open(args.detail, "w") if args.detail else sys.stdout


cost_model_file: Dict[str, TextIOWrapper] | None = (
    dict(
        [
            [target_name, open(f"{args.cost_model_prefix}_{target_name}.txt", "w")]
            for target_name in vb_targets
        ]  # type: ignore
    )  # type: ignore
    if args.cost_model_prefix
    else None
)

usecaseFolder = os.path.join(args.input, "usecases")
modules = []
for path in os.listdir(usecaseFolder):
    usecase_folder_path = os.path.join(usecaseFolder, path)
    if not os.path.isdir(usecase_folder_path):
        continue
    for path in os.listdir(usecase_folder_path):
        if path.endswith("wasm"):
            modules.append(os.path.join(usecase_folder_path, path))


class OpcodeCost:
    def __init__(self):
        self.count = defaultdict(int)
        self.costs = defaultdict(float)


opcode_cost_statistic: defaultdict[str, OpcodeCost] = defaultdict(lambda: OpcodeCost())


def analyzer_opcode_size(target_name: str, wasm: bytes, dwarf_binary: bytes):
    function_offsets = wasm_parser.get_func_offsets(wasm)
    for wasm_offset, machine_code_offsets in dwarf.decode_dwarf(dwarf_binary).items():
        if wasm_offset in function_offsets:
            wasm_opcode_str = "func"
        else:
            wasm_opcode_str = wasm_parser.get_wasm_op_code_str(wasm, wasm_offset)
        assert not wasm_opcode_str.startswith("unknown"), wasm_opcode_str
        opcode_cost_statistic[wasm_opcode_str].count[target_name] += 1
        cost = len(machine_code_offsets)
        opcode_cost_statistic[wasm_opcode_str].costs[target_name] += cost


def finalized_opcode_cost():
    print("## opcode break down\n", file=detail_file)
    cost_header = " | ".join([target_name for target_name in vb_targets])
    cost_sep = " | ".join(["----" for _ in vb_targets])
    print(f"| wasm_opcode | count | {cost_header} |", file=detail_file)
    print(f"|-------------|-------|{cost_sep}|", file=detail_file)
    statistic = list(opcode_cost_statistic.items())
    statistic.sort(key=lambda item: sum(item[1].count.values()), reverse=True)
    for wasm_opcode_str, item in statistic:
        for target_name in vb_targets:
            if item.count[target_name] > 0:
                item.costs[target_name] = (
                    item.costs[target_name] / item.count[target_name]
                )
            else:
                item.costs[target_name] = math.inf
            if cost_model_file is not None:
                print(
                    wasm_opcode_str,
                    item.costs[target_name],
                    file=cost_model_file[target_name],
                )

        final_count = sum(item.count.values()) / len(vb_targets) / len(modules)
        cost_breakdown = " | ".join(
            [f"{item.costs[target_name]:.2f}" for target_name in vb_targets]
        )
        print(
            f"|{wasm_opcode_str} | {final_count} | {cost_breakdown} |",
            file=detail_file,
        )


def run_analyze(modules):
    for target_name in vb_targets:
        vb_warp = importlib.import_module(name=target_name)
        outputs = []
        print(f"\n\n## {target_name}\n", file=detail_file)
        for wasm in modules:
            short_name = (
                os.path.split(os.path.split(wasm)[0])[1] + "/" + os.path.split(wasm)[1]
            )
            print(f"### {short_name}\n", file=detail_file)
            wasm = open(wasm, "rb").read()
            module_size = len(wasm)
            compiler = vb_warp.Compiler()
            compiler.enable_dwarf(True)
            compiler.enable_analytics(True)
            try:
                compiler.compile(wasm)
            except Exception as e:
                print(f"Error: {e}", file=detail_file)
                continue
            dwarf_binary = compiler.get_dwarf_object()
            analyzer_opcode_size(target_name, wasm, dwarf_binary)

            jit_size = compiler.get_jit_size()
            spills = compiler.get_spills_to_stack() + compiler.get_spills_to_reg()
            result = {
                "in_out_ratio": jit_size / module_size,
                "spills": spills,
            }
            outputs.append(result)
            print(
                f"module size: {module_size} jit size: {jit_size} spills: {spills}\n",
                file=detail_file,
            )
        mean_in_out_ratio = geometric_mean([o["in_out_ratio"] for o in outputs])
        mean_spills = geometric_mean([o["spills"] for o in outputs])
        print(
            f"{target_name} size score: {mean_in_out_ratio} spills score: {mean_spills}",
            file=output_file,
        )
        del vb_warp


run_analyze(modules)
finalized_opcode_cost()

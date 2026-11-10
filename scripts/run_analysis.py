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
from io import StringIO, TextIOWrapper
import math
import os
import sys
import time
from statistics import geometric_mean, mean, median, pstdev
import importlib
from helper import dwarf, wasm_parser
from collections import defaultdict
from concurrent.futures import ProcessPoolExecutor
from multiprocessing import get_context
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
parser.add_argument(
    "--repeat",
    type=int,
    default=1,
    help="Number of times to compile each module",
)
parser.add_argument(
    "--max-workers",
    type=int,
    default=len(vb_targets),
    help="Maximum number of target analysis workers",
)

args = parser.parse_args()
if args.repeat < 1:
    parser.error("--repeat must be at least 1")
if args.max_workers < 1:
    parser.error("--max-workers must be at least 1")


def open_output(path):
    if not path:
        return sys.stdout
    output_directory = os.path.dirname(path)
    if output_directory:
        os.makedirs(output_directory, exist_ok=True)
    return open(path, "w")


output_file = open_output(args.output)
detail_file = open_output(args.detail)


cost_model_file: Dict[str, TextIOWrapper] | None = (
    dict(
        [
            [target_name, open_output(f"{args.cost_model_prefix}_{target_name}.txt")]
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


def run_target_analysis(target_name: str, modules: list[str], repeat: int):
    global opcode_cost_statistic

    opcode_cost_statistic = defaultdict(OpcodeCost)
    vb_warp = importlib.import_module(name=target_name)
    detail_output = StringIO()
    outputs = []
    print(f"\n\n## {target_name}\n", file=detail_output)
    for wasm in modules:
        short_name = (
            os.path.split(os.path.split(wasm)[0])[1] + "/" + os.path.split(wasm)[1]
        )
        print(f"### {short_name}\n", file=detail_output)
        wasm = open(wasm, "rb").read()
        module_size = len(wasm)
        compile_times_ms = []
        module_compile_result = None
        for repeat_index in range(repeat):
            compiler = vb_warp.Compiler()
            compiler.enable_dwarf(True)
            compiler.enable_analytics(True)
            compile_start_ns = time.perf_counter_ns()
            try:
                compiler.compile(wasm)
            except Exception as e:
                print(
                    f"Compile {repeat_index + 1}/{repeat} failed: {e}",
                    file=detail_output,
                )
                continue

            compile_times_ms.append(
                (time.perf_counter_ns() - compile_start_ns) // 1_000_000
            )
            if module_compile_result is None:
                dwarf_binary = compiler.get_dwarf_object()
                analyzer_opcode_size(target_name, wasm, dwarf_binary)

                jit_size = compiler.get_jit_size()
                spills = compiler.get_spills_to_stack() + compiler.get_spills_to_reg()
                module_compile_result = {
                    "in_out_ratio": jit_size / module_size,
                    "spills": spills,
                }

        if not compile_times_ms:
            continue
        outputs.append(module_compile_result)
        print(
            f"compile time (ms): mean={mean(compile_times_ms):.2f}, "
            f"median={median(compile_times_ms):.2f}, "
            f"standard deviation={pstdev(compile_times_ms):.2f} "
            f"(n={len(compile_times_ms)})\n"
            f"compile time (ms): min={min(compile_times_ms)}, "
            f"max={max(compile_times_ms)}\n"
            f"module size: {module_size} jit size: {jit_size} spills: {spills}\n",
            file=detail_output,
        )
    mean_in_out_ratio = geometric_mean([o["in_out_ratio"] for o in outputs])
    mean_spills = geometric_mean([o["spills"] for o in outputs])
    summary = (
        f"{target_name} size score: {mean_in_out_ratio} "
        f"spills score: {mean_spills}\n"
    )
    target_opcode_costs = {
        opcode: (item.count[target_name], item.costs[target_name])
        for opcode, item in opcode_cost_statistic.items()
    }
    return target_name, detail_output.getvalue(), summary, target_opcode_costs


def run_analyze(modules):
    with ProcessPoolExecutor(
        max_workers=args.max_workers, mp_context=get_context("fork")
    ) as executor:
        workers = [
            executor.submit(run_target_analysis, target_name, modules, args.repeat)
            for target_name in vb_targets
        ]
        for worker in workers:
            target_name, detail, summary, target_opcode_costs = worker.result()
            detail_file.write(detail)
            output_file.write(summary)
            for opcode, (count, total_cost) in target_opcode_costs.items():
                opcode_cost = opcode_cost_statistic[opcode]
                opcode_cost.count[target_name] = count
                opcode_cost.costs[target_name] = total_cost


run_analyze(modules)
finalized_opcode_cost()

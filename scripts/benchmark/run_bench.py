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

import re
import os
import signal, sys
import argparse
import statistics
import subprocess
import math
import shutil
from colorama import Fore
from statistics import geometric_mean

signal.signal(signal.SIGINT, lambda x, y: sys.exit(0))


def get_executable_path(executable_name):
    """Get the full path of an executable using 'which' command."""
    try:
        # Use shutil.which as a more portable way to find executables
        full_path = shutil.which(executable_name)
        if full_path:
            return full_path

        # Fallback to subprocess if shutil.which fails
        result = subprocess.run(
            ["which", executable_name],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )
        if result.returncode == 0:
            return result.stdout.strip()
    except Exception as e:
        print(f"Warning: Could not find full path for {executable_name}: {e}")

    # If all else fails, return the original name
    return executable_name


parser = argparse.ArgumentParser(
    description="Helper script to automatically run benchmarks for Wasm runtimes"
)
parser.add_argument(
    "-i", "--input", help="Path to the wasm-compiler-benchmark folder", required=True
)
parser.add_argument("-x", "--executable", help="Path to the executable", required=True)
parser.add_argument(
    "-n",
    help="Number of times each benchmark should be executed",
    required=False,
    default=1,
)
args = vars(parser.parse_args())

script_dir = sys.path[0]

n = int(args["n"])
input_folder = args["input"]
executable = args["executable"]
d8_path = get_executable_path("d8")


def get_files(folder):
    return [os.path.join(folder, file) for file in os.listdir(folder)]


def remove_ansi(string):
    ansi_pattern = r"(?:\x1B[@-_]|[\x80-\x9F])[0-?]*[ -/]*[@-~]"
    return re.sub(ansi_pattern, "", str(string))


def pad(string, n, add_space=True):
    string = str(string)
    len_str = len(remove_ansi(string))
    if len_str >= n:
        return string
    return string + " " * (n - len_str) + (" " if add_space and len_str > 0 else "")


def color_threshold(str_val, threshold=0, less_is_better=True, unit=""):
    float_val = float(str_val)
    is_better = float_val <= threshold if less_is_better else float_val >= threshold
    fore = Fore.GREEN if is_better else Fore.RED
    return fore + str_val + unit + Fore.RESET


def format_delta_perc(val, base, less_is_better=True):
    reldiff = 100 * (val / base - 1)
    return color_threshold(f"{reldiff:+.1f}", 0, less_is_better, "%")


def print_column(string, width):
    print(pad(string, width), end="")


def finish_columns():
    print()


def all_same(items):
    return all(x == items[0] for x in items)


def run_benchmarks(cmd, modules, output_patterns=None, baseline=None):
    if not output_patterns:
        output_patterns = {}
    modules.sort()
    module_info = {}

    print_column("Name", 40)
    print_column("Mean (s)", 9)
    print_column("Stddev", 7)
    if baseline:
        print_column("Baseline", 9)
        if "result" in output_patterns:
            print_column("Result", 7)
    if "compilation_time_ms" in output_patterns:
        print_column("Comptime", 9)
    if "execution_time_ms" in output_patterns:
        print_column("Exectime", 9)
    if (
        "compilation_time_ms" in output_patterns
        and "execution_time_ms" in output_patterns
    ):
        print_column("Setup", 9)
    print_column("Runs", 5)
    finish_columns()

    for module in modules:
        basename = os.path.basename(module)
        stem = os.path.splitext(basename)[0]
        base_cmd = cmd.replace("__MODULE__", module)
        base_cmd = base_cmd.replace("__SCRIPTDIR__", script_dir)

        full_cmd = f"/usr/bin/time -p {base_cmd}"

        # if stem == "atax": break
        module_info[stem] = {}

        exec_times = []
        filtered_results = {}
        for i in range(n):
            print(f"{basename} ({i}/{n})", end="\r")

            pipes = subprocess.Popen(
                full_cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True
            )
            stdout, stderr = pipes.communicate()
            print((str(stderr)))
            match = re.search(r"real\s+(\d+\.\d+)", str(stderr))
            exec_time = float(match.group(1))
            exec_times.append(exec_time)

            for key in output_patterns:
                pattern = output_patterns[key]
                output_match = re.search(pattern, str(stdout), flags=re.IGNORECASE)

                if not output_match:
                    print(f"ERROR: {stdout} did not match pattern {pattern}")

                if key not in filtered_results:
                    filtered_results[key] = []
                filtered_results[key].append(output_match.group(0))

        mean = statistics.mean(exec_times)
        relstddev_perc = 100 * statistics.stdev(exec_times) / mean if n > 1 else 0

        module_info[stem]["mean"] = mean
        if "result" in output_patterns:
            assert all_same(filtered_results["result"])
            module_info[stem]["result"] = filtered_results["result"][0]
        if "compilation_time_ms" in output_patterns:
            module_info[stem]["compilation_time_ms"] = statistics.mean(
                [float(x) for x in filtered_results["compilation_time_ms"]]
            )
        if "execution_time_ms" in output_patterns:
            module_info[stem]["execution_time_ms"] = statistics.mean(
                [float(x) for x in filtered_results["execution_time_ms"]]
            )

        print_column(basename, 40)
        print_column("{:.3f}".format(mean), 9)
        print_column(color_threshold(f"{relstddev_perc:.2f}", 3, True, "%"), 7)
        if baseline:
            print_column(
                format_delta_perc(mean, baseline["module_info"][stem]["mean"]), 9
            )

            if "result" in output_patterns:
                status = "FAR"
                baseline_result = baseline["module_info"][stem]["result"]
                current_result = module_info[stem]["result"]
                if baseline_result == current_result:
                    status = "MATCH"
                elif math.isclose(
                    float(baseline_result), float(current_result), rel_tol=1e-4
                ):
                    status = "CLOSE"
                print_column(
                    (Fore.RED if status == "FAR" else Fore.GREEN) + status + Fore.RESET,
                    7,
                )

        comp_time_fraction = 0
        exec_time_fraction = 0
        mean_ms = 1000 * module_info[stem]["mean"]
        if "compilation_time_ms" in output_patterns:
            comp_time_fraction = module_info[stem]["compilation_time_ms"] / mean_ms
            print_column(
                color_threshold(f"{100 * comp_time_fraction:.3f}", 0.02, True, "%"), 9
            )
        if "execution_time_ms" in output_patterns:
            exec_time_fraction = module_info[stem]["execution_time_ms"] / mean_ms
            print_column(
                color_threshold(f"{100 * exec_time_fraction:.3f}", 95, False, "%"), 9
            )
        if (
            "compilation_time_ms" in output_patterns
            and "execution_time_ms" in output_patterns
        ):
            other_time_fraction = 1.0 - comp_time_fraction - exec_time_fraction
            print_column(
                color_threshold(f"{100 * other_time_fraction:.3f}", 4, True, "%"), 9
            )
        print_column(n, 5)
        finish_columns()

    geom_mean = geometric_mean([module_info[name]["mean"] for name in module_info])
    score = int(10000 / geom_mean)

    if baseline:
        score_perc = format_delta_perc(score, baseline["score"], less_is_better=False)
        slowdown = [
            (
                module_info[module_name]["mean"]
                - baseline["module_info"][module_name]["mean"]
            )
            / module_info[module_name]["mean"]
            for module_name in module_info
        ]
        mean_slowdown = statistics.mean(slowdown)
        slowdown_perc = color_threshold(f"{100 * mean_slowdown:.1f}", 0, True, "%")
        print(
            f"\nOverall score: {score} ({score_perc}), mean slowdown: " + slowdown_perc
        )
    else:
        print(f"\nOverall score: {score}")

    res = {"score": score, "module_info": module_info}
    return res


#
#
#

compilation_wasm_folder = os.path.join(input_folder, "compilation", "build")
execution_wasm_folder = os.path.join(input_folder, "execution", "build", "wasm")
execution_native_folder = os.path.join(input_folder, "execution", "build", "native")

print("\n--- VB COMPILATION BENCHMARK")
vb_comp_results = run_benchmarks(
    f"{executable} __MODULE__", get_files(compilation_wasm_folder)
)

print(f"\n--- V8 Liftoff Wasm COMPILATION (BASELINE=VB)")
run_benchmarks(
    f"{d8_path} --no-wasm-lazy-compilation --liftoff --single-threaded __SCRIPTDIR__/util/d8_harness.js -- __MODULE__",
    get_files(compilation_wasm_folder),
    None,
    vb_comp_results,
)

print(f"\n--- V8 TurboFan Wasm COMPILATION (BASELINE=VB)")
run_benchmarks(
    f"{d8_path} --no-wasm-lazy-compilation --no-liftoff --single-threaded __SCRIPTDIR__/util/d8_harness.js -- __MODULE__",
    get_files(compilation_wasm_folder),
    None,
    vb_comp_results,
)

js_path = get_executable_path("js")
print(f"\n--- SpiderMonkey Baseline Wasm COMPILATION (BASELINE=VB)")
run_benchmarks(
    f"{js_path} --wasm-compiler=baseline --cpu-count=1 --delazification-mode=eager __SCRIPTDIR__/util/sm_harness.js __MODULE__",
    get_files(compilation_wasm_folder),
    None,
    vb_comp_results,
)

print(f"\n--- SpiderMonkey Ion Wasm COMPILATION (BASELINE=VB)")
run_benchmarks(
    f"{js_path} --wasm-compiler=ion --cpu-count=1 --delazification-mode=eager __SCRIPTDIR__/util/sm_harness.js __MODULE__",
    get_files(compilation_wasm_folder),
    None,
    vb_comp_results,
)


print("\n--- NATIVE EXECUTION")
native_output_patterns = {"result": r"(?<=RES )-?(nan|inf|\d+\.\d+)"}
native_exec_results = run_benchmarks(
    "__MODULE__", get_files(execution_native_folder), native_output_patterns
)

print(f"\n--- VB Wasm EXECUTION {executable} (BASELINE=NATIVE)")
wasm_output_patterns = {
    "result": r"(?<=RES )-?(nan|inf|\d+\.\d+)",
    "compilation_time_ms": r"(?<=Compilation time \(ms\): )\d+\.\d+",
    "execution_time_ms": r"(?<=Execution time \(ms\): )\d+\.\d+",
}
vb_exec_results = run_benchmarks(
    f"{executable} __MODULE__ gen_start",
    get_files(execution_wasm_folder),
    wasm_output_patterns,
    native_exec_results,
)

print(f"\n--- V8 Liftoff Wasm EXECUTION (BASELINE=VB)")
run_benchmarks(
    f"{d8_path} --no-wasm-lazy-compilation --liftoff --single-threaded __SCRIPTDIR__/util/d8_harness.js -- __MODULE__ gen_start",
    get_files(execution_wasm_folder),
    wasm_output_patterns,
    vb_exec_results,
)

print(f"\n--- V8 TurboFan Wasm EXECUTION (BASELINE=VB)")
run_benchmarks(
    f"{d8_path} --no-wasm-lazy-compilation --no-liftoff --single-threaded __SCRIPTDIR__/util/d8_harness.js -- __MODULE__ gen_start",
    get_files(execution_wasm_folder),
    wasm_output_patterns,
    vb_exec_results,
)

print(f"\n--- SpiderMonkey Baseline Wasm EXECUTION (BASELINE=VB)")
run_benchmarks(
    f"{js_path} --wasm-compiler=baseline --cpu-count=1 --delazification-mode=eager __SCRIPTDIR__/util/sm_harness.js __MODULE__ gen_start",
    get_files(execution_wasm_folder),
    wasm_output_patterns,
    vb_exec_results,
)

print(f"\n--- SpiderMonkey Ion Wasm EXECUTION (BASELINE=VB)")
run_benchmarks(
    f"{js_path} --wasm-compiler=ion --cpu-count=1 --delazification-mode=eager __SCRIPTDIR__/util/sm_harness.js __MODULE__ gen_start",
    get_files(execution_wasm_folder),
    wasm_output_patterns,
    vb_exec_results,
)

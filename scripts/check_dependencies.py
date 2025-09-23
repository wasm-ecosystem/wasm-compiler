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

import os

output_file_path = "check.ll"

# from https://github.com/llvm/llvm-project/tree/main/compiler-rt/lib/sanitizer_common/scripts/gen_dynamic_list.py
new_delete_std_symbols = [
    ["_Znam", "operator new[](unsigned long)"],
    ["_ZnamRKSt9nothrow_t", "operator new[](unsigned long)"],
    ["_Znwm", "operator new(unsigned long)"],
    ["_ZnwmRKSt9nothrow_t", "operator new(unsigned long)"],
    ["_Znaj", "operator new[](unsigned int)"],
    ["_ZnajRKSt9nothrow_t", "operator new[](unsigned int)"],
    ["_Znwj", "operator new(unsigned int)"],
    ["_ZnwjRKSt9nothrow_t", "operator new(unsigned int)"],
    ["_ZnwmSt11align_val_t", "operator new(unsigned long, std::align_val_t)"],
    [
        "_ZnwmSt11align_val_tRKSt9nothrow_t",
        "operator new(unsigned long, std::align_val_t)",
    ],
    ["_ZnwjSt11align_val_t", "operator new(unsigned int, std::align_val_t)"],
    [
        "_ZnwjSt11align_val_tRKSt9nothrow_t",
        "operator new(unsigned int, std::align_val_t)",
    ],
    ["_ZnamSt11align_val_t", "operator new[](unsigned long, std::align_val_t)"],
    [
        "_ZnamSt11align_val_tRKSt9nothrow_t",
        "operator new[](unsigned long, std::align_val_t)",
    ],
    ["_ZnajSt11align_val_t", "operator new[](unsigned int, std::align_val_t)"],
    [
        "_ZnajSt11align_val_tRKSt9nothrow_t",
        "operator new[](unsigned int, std::align_val_t)",
    ],
    ["_ZdaPv", "operator delete[](void *)"],
    ["_ZdaPvRKSt9nothrow_t", "operator delete[](void *)"],
    # TODO(disable due to exception)
    # ['_ZdlPv', 'operator delete(void *)'],
    ["_ZdlPvRKSt9nothrow_t", "operator delete(void *)"],
    ["_ZdaPvm", "operator delete[](void*, unsigned long)"],
    ["_ZdlPvm", "operator delete(void*, unsigned long)"],
    ["_ZdaPvj", "operator delete[](void*, unsigned int)"],
    ["_ZdlPvj", "operator delete(void*, unsigned int)"],
    ["_ZdlPvSt11align_val_t", "operator delete(void*, std::align_val_t)"],
    ["_ZdlPvSt11align_val_tRKSt9nothrow_t", "operator delete(void*, std::align_val_t)"],
    ["_ZdaPvSt11align_val_t", "operator delete[](void*, std::align_val_t)"],
    [
        "_ZdaPvSt11align_val_tRKSt9nothrow_t",
        "operator delete[](void*, std::align_val_t)",
    ],
    [
        "_ZdlPvmSt11align_val_t",
        "operator delete(void*, unsigned long,  std::align_val_t)",
    ],
    [
        "_ZdaPvmSt11align_val_t",
        "operator delete[](void*, unsigned long, std::align_val_t)",
    ],
    [
        "_ZdlPvjSt11align_val_t",
        "operator delete(void*, unsigned int,  std::align_val_t)",
    ],
    [
        "_ZdaPvjSt11align_val_t",
        "operator delete[](void*, unsigned int, std::align_val_t)",
    ],
]

diagnose = []


def find_all_files(base):
    for root, ds, fs in os.walk(base):
        for f in fs:
            yield os.path.join(root, f)


def diagnose_error_file(error_symbol, file_name):
    diagnose.append(
        "find {} aka {} in {}".format(error_symbol[0], error_symbol[1], file_name)
    )
    current_def = "unknown"
    with open(output_file_path) as f:
        for line in f.readlines():
            if line.startswith("define "):
                current_def = line[:-1]
            elif line.startswith("declare "):
                current_def = "unknown"
            elif error_symbol[0] in line:
                diagnose.append("    in {}".format(current_def))
    diagnose.append("")


def runCheck(file_name):
    os.system("clang {} -S -emit-llvm -I. -o {}".format(file_name, output_file_path))
    with open(output_file_path) as f:
        for line in f.readlines():
            if line.startswith("declare "):
                for symbol in new_delete_std_symbols:
                    if ("@" + symbol[0] + "(") in line:
                        diagnose_error_file(symbol, file_name)


def main():
    base = "src/core"
    for file_name in find_all_files(base):
        if file_name.endswith(".cpp"):
            runCheck(file_name)
    if len(diagnose) != 0:
        print("\n".join(diagnose))
        exit(-1)


if __name__ == "__main__":
    main()

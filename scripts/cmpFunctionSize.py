#!/usr/bin/env python3
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


"""
Helper script to compare function size in disassembly. 
It's helpful to know which function size changed when code size changed in benchmarks.
Example usage:
1. Checkout old code
2. python ./scripts/warp_dump.py --target x86_64 --module path_to_test_case.wasm > old.txt
3. Checkout new code
4. python ./scripts/warp_dump.py --target x86_64 --module path_to_test_case.wasm > new.txt
5. python ./scripts/cmpFunctionSize.py old.txt new.txt
"""

import re
import os
import argparse


def extract_function_sizes(filename):
    """Extract lines containing function sizes and their line numbers from a file."""
    sizes = []
    line_numbers = []

    with open(filename, "r") as f:
        for i, line in enumerate(f, 1):
            match = re.search(r"Size of the function body:\s+(\d+)", line)
            if match:
                sizes.append(int(match.group(1)))
                line_numbers.append(i)

    return sizes, line_numbers


def main():
    # Create argument parser
    parser = argparse.ArgumentParser(
        description="Compare function sizes between two files."
    )
    parser.add_argument("old_file", help="Path to the old file")
    parser.add_argument("new_file", help="Path to the new file")

    args = parser.parse_args()
    old_file = args.old_file
    new_file = args.new_file

    # Check if files exist
    if not os.path.exists(old_file):
        print(f"Error: {old_file} does not exist")
        return
    if not os.path.exists(new_file):
        print(f"Error: {new_file} does not exist")
        return

    # Extract function sizes and line numbers
    old_sizes, old_lines = extract_function_sizes(old_file)
    new_sizes, new_lines = extract_function_sizes(new_file)

    # Compare sizes
    min_length = min(len(old_sizes), len(new_sizes))

    print(
        f"Found {len(old_sizes)} functions in old file and {len(new_sizes)} functions in new file"
    )

    differences = 0
    for i in range(min_length):
        if old_sizes[i] != new_sizes[i]:
            differences += 1
            print(
                f"Function #{i+1} differs: old[line {old_lines[i]}]={old_sizes[i]}, new[line {new_lines[i]}]={new_sizes[i]}, diff={new_sizes[i] - old_sizes[i]}"
            )

    # Check for extra functions
    if len(old_sizes) > len(new_sizes):
        print(f"\nOld file has {len(old_sizes) - len(new_sizes)} more functions")
        for i in range(min_length, len(old_sizes)):
            print(
                f"Extra function in old file at line {old_lines[i]}: size={old_sizes[i]}"
            )
    elif len(new_sizes) > len(old_sizes):
        print(f"\nNew file has {len(new_sizes) - len(old_sizes)} more functions")
        for i in range(min_length, len(new_sizes)):
            print(
                f"Extra function in new file at line {new_lines[i]}: size={new_sizes[i]}"
            )

    if differences == 0 and len(old_sizes) == len(new_sizes):
        print("No differences found")
    else:
        print(f"\nFound {differences} functions with different sizes")


if __name__ == "__main__":
    main()

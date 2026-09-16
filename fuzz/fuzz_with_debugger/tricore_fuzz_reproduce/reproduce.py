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
import subprocess


def convert_wat_to_wasm(wat_file, wasm_file):
    subprocess.run(["wat2wasm", wat_file, "-o", wasm_file], check=True)


def read_wasm_file(wasm_file):
    with open(wasm_file, "rb") as f:
        return f.read()


def create_cpp_file(wasm_bytes, cpp_file):
    hex_bytes = ", ".join(f"0x{byte:02x}" for byte in wasm_bytes)
    array_size = len(wasm_bytes)
    cpp_content = f"""// clang-format off
#include <array>
#include <cstddef>
#include <cstdint>

constexpr std::array<uint8_t, {array_size}> bytecode = {{{hex_bytes}}};

const uint8_t* bytecodeStart = bytecode.data();
size_t bytecodeLength = bytecode.size();
"""
    with open(cpp_file, "w") as f:
        f.write(cpp_content)


def main():
    wat_file = "reproduce.wat"
    wasm_file = "reproduce.wasm"
    cpp_file = "reproduce.cpp"

    # Convert WAT to WASM
    convert_wat_to_wasm(wat_file, wasm_file)

    # Read the WASM file
    wasm_bytes = read_wasm_file(wasm_file)

    # Create the C++ file
    create_cpp_file(wasm_bytes, cpp_file)

    # Set project root and build directory
    project_root = os.path.join("..", "..", "..")
    build_dir = os.path.join(project_root, "build_tricore")

    os.makedirs(build_dir, exist_ok=True)

    # Read environment variables
    tricore_gcc_path = os.environ["TRICORE_GCC_PATH"]
    tricore_qemu_path = os.environ["TRICORE_QEMU_PATH"]

    # Run CMake
    cmake_command = [
        "cmake",
        "..",
        "-DENABLE_FUZZ=1",
        "-DFUZZ_ONLY_WITH_DEBUGGER=1",
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DVB_ENABLE_DEV_FEATURE=OFF",
        f"-DCMAKE_C_COMPILER={tricore_gcc_path}/tricore-elf-gcc",
        f"-DCMAKE_CXX_COMPILER={tricore_gcc_path}/tricore-elf-g++",
    ]
    subprocess.run(cmake_command, cwd=build_dir, check=True)

    # Run make
    subprocess.run(
        ["cmake", "--build", ".", "--target", "tricore_fuzz_reproduce", "--parallel"],
        cwd=build_dir,
        check=True,
    )

    # Run QEMU
    print("-----------run qemu------------------------")
    qemu_command = [
        f"{tricore_qemu_path}/qemu-system-tricore",
        "-semihosting",
        "-display",
        "none",
        "-M",
        "tricore_tsim162",
        "-kernel",
        os.path.join(build_dir, "bin", "tricore_fuzz_reproduce"),
    ]
    subprocess.run(qemu_command, check=True)

    print("-----------run wasm-interp------------------------")

    # Run wasm-interp
    wasm_interp_command = [
        "wasm-interp",
        "--dummy-import-func",
        "--run-all-exports",
        wasm_file,
    ]
    subprocess.run(wasm_interp_command, check=True)


if __name__ == "__main__":
    main()

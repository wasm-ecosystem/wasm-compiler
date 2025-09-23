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

from importlib.machinery import EXTENSION_SUFFIXES
import subprocess
import argparse
import shutil
import os
import sys
from typing import List


PY_SO_EXT = EXTENSION_SUFFIXES[0]
PROJECT_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))


parser = argparse.ArgumentParser()
parser.add_argument(
    "--cmake-args", nargs="*", help="Arguments to pass to CMake", default=[]
)
parser.add_argument("--venv-dir", default="venv", help="Path to the venv directory")
parser.add_argument(
    "--debug", default=False, action="store_true", help="Enable debug build"
)
parser.add_argument("--backend", default=None, help="Special target")

args = parser.parse_args()

TARGET = [
    ("x86_64", True),
    ("x86_64", False),
    ("aarch64", True),
    ("aarch64", False),
    ("tricore", False),
    (None, False),
]


def get_backend_identifier(backend: str | None, is_active_mode: bool) -> str:
    if backend is None:
        return "native"
    else:
        backend_identifier = backend
        if is_active_mode:
            backend_identifier += "_active"
        return backend_identifier


def build(backend: str | None, is_active_mode: bool):
    cmake_args: List[str] = args.cmake_args.copy()
    if args.debug:
        cmake_args.append("-DCMAKE_BUILD_TYPE=Debug")
    cxx_flags = []
    backend_identifier = get_backend_identifier(backend, is_active_mode)
    if backend is None:
        macro_name = "vb"
    else:
        cmake_args += [
            f"-DBACKEND={backend}",
        ]
        if is_active_mode:
            cxx_flags += [
                "-DNO_PASSIVE_PROTECTION_WARNING",
                "-DLINEAR_MEMORY_BOUNDS_CHECKS=1",
                "-DACTIVE_STACK_OVERFLOW_CHECK=1",
                "-DACTIVE_DIV_CHECK=1",
            ]
        macro_name = f"{backend_identifier}_vb"
        cxx_flags += [
            f"-Dvb={macro_name}",
        ]
    package_name = f"{macro_name}_warp"
    cxx_flags += [
        f"-DVB_BINDING_NAME={package_name}",
    ]

    build_dir = f"build_binding_{backend_identifier}"
    cxx_flags_str = " ".join(cxx_flags)
    print(cmake_args)
    subprocess.run(
        [
            "cmake",
            "-B",
            build_dir,
            "-S",
            PROJECT_DIR,
            "-DENABLE_BINDING=1",
            "-DVB_ENABLE_DEV_FEATURE=OFF",
            f"-DCMAKE_CXX_FLAGS={cxx_flags_str}",
        ]
        + cmake_args,
        check=True,
    )
    subprocess.run(
        [
            "cmake",
            "--build",
            build_dir,
            "--target",
            "vb_warp",
            "--parallel",
        ],
        check=True,
    )
    so_path = os.path.join(
        args.venv_dir,
        "lib",
        f"python{sys.version_info[0]}.{sys.version_info[1]}",
        "site-packages",
        f"{package_name}{PY_SO_EXT}",
    )
    shutil.copyfile(
        os.path.join(build_dir, "binding", "python", f"vb_warp{PY_SO_EXT}"),
        so_path,
    )
    print(f"Successfully built {package_name} in {so_path}")


for backend, is_active_mode in TARGET:
    backend_identifier = get_backend_identifier(backend, is_active_mode)
    if args.backend is not None and backend_identifier != args.backend:
        continue
    build(backend=backend, is_active_mode=is_active_mode)

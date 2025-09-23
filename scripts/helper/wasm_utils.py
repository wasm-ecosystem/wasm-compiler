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

import subprocess


def wat_to_wasm(*, path: str | None = None, wat: bytes | None = None) -> bytes:
    """
    generate wasm binary from wat file or wat string
    """
    if path is not None:
        assert wat is None
        proc = subprocess.run(
            f"wasm-tools parse --generate-dwarf=lines -o /dev/stdout '{path}'",
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
        )
        if proc.returncode != 0:
            raise Exception(proc.stderr.decode())
        return proc.stdout
    elif wat is not None:
        assert path is None
        proc = subprocess.run(
            f"wasm-tools parse --generate-dwarf=lines -o /dev/stdout /dev/stdin",
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            input=wat,
        )
        if proc.returncode != 0:
            raise Exception(proc.stderr.decode())
        return proc.stdout
    else:
        assert False


def load_wasm_or_wat(path: str) -> bytes:
    """
    load wasm binary from path
    """
    if path.endswith(".wasm"):
        with open(path, "rb") as f:
            return f.read()
    elif path.endswith(".wat"):
        return wat_to_wasm(path=path)
    else:
        raise Exception("Invalid wasm extension")

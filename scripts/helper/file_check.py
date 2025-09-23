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

import tempfile
import os
import subprocess
from typing import List, Dict, Tuple


def convert_config_to_file_check_short_prefix(config: Dict[str, str]) -> str:
    if (
        config["ACTIVE_STACK_OVERFLOW_CHECK"] == "0"
        and config["LINEAR_MEMORY_BOUNDS_CHECKS"] == "0"
    ):
        return f"{config["BACKEND"]}_PASSIVE"
    elif (
        config["ACTIVE_STACK_OVERFLOW_CHECK"] == "1"
        and config["LINEAR_MEMORY_BOUNDS_CHECKS"] == "1"
    ):
        return f"{config["BACKEND"]}_ACTIVE"
    assert False


def convert_config_to_file_check_prefix(config: Dict[str, str]) -> List[str]:
    file_check_prefix = ["CHECK", config["BACKEND"]]
    for config_item, config_value in config.items():
        if config_item != "BACKEND":
            file_check_prefix.append(
                config["BACKEND"]
                + ("_" if int(config_value) == 1 else "_NO_")
                + config_item
            )
    simple_prefix = convert_config_to_file_check_short_prefix(config)
    if simple_prefix is not None:
        file_check_prefix.append(simple_prefix)
    return file_check_prefix


def check_file(
    file: str, expected: str, file_check_prefix: List[str], is_color: bool
) -> Tuple[int, str, str]:
    tmp_fd, tmp_path = tempfile.mkstemp()
    with os.fdopen(tmp_fd, "w") as tmp_file:
        tmp_file.write(expected)

    file_check_prefix_str = ",".join(file_check_prefix)
    cmd = [
        "FileCheck",
        file,
        f"--input-file={tmp_path}",
        f"--check-prefixes={file_check_prefix_str}",
        "--allow-unused-prefixes",
    ]
    if is_color:
        cmd.append("--color")

    p = subprocess.Popen(
        cmd,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )
    return_code = p.wait()
    if return_code == 0:
        os.remove(tmp_path)
    assert p.stdout is not None and p.stderr is not None
    return return_code == 0, p.stdout.read().decode(), p.stderr.read().decode()

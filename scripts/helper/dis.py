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

from typing import List, Dict, Tuple


def parse_config_str(config_str) -> Dict[str, str]:
    config = {}
    for config_item_str in config_str.split(" "):
        if "=" in config_item_str:
            config_item = config_item_str.split("=")
            config[config_item[0]] = config_item[1]
    assert "BACKEND" in config
    return config


def process_dis_output(
    dis_lines: str, config: Dict[str, str], has_memory: bool
) -> Tuple[str, List[int]]:
    dis_lines_list: List[str] = dis_lines.split("\n")
    need_to_replace_trap_handle = True
    need_to_replace_landing_pad = config["LINEAR_MEMORY_BOUNDS_CHECKS"] == "0"
    need_to_replace_extension_request = (
        config["LINEAR_MEMORY_BOUNDS_CHECKS"] == "1" and has_memory
    )
    cnt = 0
    func_positions = []
    for i in range(len(dis_lines_list)):
        if "Function or wrapper body, padded to 4B" in dis_lines_list[i]:
            if need_to_replace_trap_handle:
                need_to_replace_trap_handle = False
                dis_lines_list[i] = dis_lines_list[i].replace(
                    f"Function or wrapper body, padded to 4B", "GenericTrapHandler Body"
                )
            elif need_to_replace_landing_pad:
                need_to_replace_landing_pad = False
                dis_lines_list[i] = dis_lines_list[i].replace(
                    f"Function or wrapper body, padded to 4B", "LandingPad Body"
                )
            elif need_to_replace_extension_request:
                need_to_replace_extension_request = False
                dis_lines_list[i] = dis_lines_list[i].replace(
                    f"Function or wrapper body, padded to 4B", "ExtensionRequest Body"
                )
            else:
                func_positions.append(i)
                dis_lines_list[i] = dis_lines_list[i].replace(
                    "Function or wrapper body, padded to 4B", f"Function[{cnt}] Body"
                )
                cnt += 1
        elif dis_lines_list[i] == "":
            func_positions.append(i)
            break
    return ("\n".join(dis_lines_list), func_positions)

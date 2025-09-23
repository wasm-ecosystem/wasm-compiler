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
from typing import List, Dict, Tuple
from http.server import BaseHTTPRequestHandler, HTTPServer
from helper import dis, wat_parser, wasm_utils, dwarf
import json
import argparse
import logging

import vb_warp
import aarch64_vb_warp
import aarch64_active_vb_warp
import x86_64_vb_warp
import x86_64_active_vb_warp
import tricore_vb_warp

targets = {
    "native": vb_warp,
    "aarch64": aarch64_vb_warp,
    "aarch64_active": aarch64_active_vb_warp,
    "x86_64": x86_64_vb_warp,
    "x86_64_active": x86_64_active_vb_warp,
    "tricore": tricore_vb_warp,
}

for _, module in targets.items():
    module.enable_color(False)


workspace = os.path.abspath(os.path.join(__file__, "..", ".."))

index_path = os.path.join(workspace, "scripts", "explorer.html")


def create_error_response_json(msg: str) -> Dict[str, str]:
    return {"error": msg}


def analyze_wat(wat: str) -> List[Dict[str, Tuple[int, int]]]:
    func_ranges = wat_parser.extract_func(wat)
    lines = wat.split("\n")
    return [
        {
            "start": wat_parser.offset_to_position(lines, func_range.start),
            "end": wat_parser.offset_to_position(lines, func_range.end),
        }
        for func_range in func_ranges
    ]


def disassemble_wat(module, wat: bytes) -> dict:
    if len(wat) == 0:
        return create_error_response_json("Empty input")
    try:
        wasm = wasm_utils.wat_to_wasm(wat=wat)
    except Exception as e:
        return create_error_response_json(
            "Failed to compile the input to wasm\n" + str(e)
        )
    compiler = module.Compiler()
    compiler.set_stacktrace_record_count(1)
    compiler.enable_dwarf(True)
    try:
        dis_lines = compiler.disassemble_wasm(wasm)
    except Exception as e:
        return create_error_response_json("Failed to compile wasm\n" + str(e))
    dwo: bytes = compiler.get_dwarf_object()
    config: Dict[str, str] = dis.parse_config_str(module.get_configuration())
    dis_output, dis_func_positions = dis.process_dis_output(
        dis_lines=dis_lines,
        config=config,
        has_memory=wat_parser.has_memory(wat.decode()),
    )
    wat_str: str = wat.decode()
    return {
        "config": config,
        "text": dis_output,
        "mapping": {
            "wat_func": analyze_wat(wat=wat_str),
            "disassembly_func": dis_func_positions,
            "debug_line": dwarf.analyze_debug_info_in_dwarf(
                dwo=dwo,
                wasm=wasm,
                assembly=dis_output,
            ),
        },
    }


class ExplorerServer(BaseHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, **kwargs)

    def do_GET(self):
        if self.path == "/":
            self.send_response(200)
            self.send_header("Content-type", "text/html")
            self.end_headers()
            index_content = open(index_path, mode="rb").read()
            self.wfile.write(index_content)
            return
        else:
            self.send_response(404)
            self.end_headers()
            return

    def do_POST(self):
        DIS_POST_PREFIX = "/api/dis"
        if self.path.startswith(DIS_POST_PREFIX):
            self.send_response(200)
            self.send_header("Content-type", "text/json")
            self.end_headers()
            response = self.do_POST_api_dis(self.path[len(DIS_POST_PREFIX) :])
            self.wfile.write(json.dumps(response).encode())
            return
        assert False

    def do_POST_api_dis(self, rest_path: str) -> Dict[str, str]:
        if rest_path.startswith("/"):
            rest_path = rest_path[1:]
        target_name = rest_path
        if target_name == "":
            target_name = "native"
        if target_name not in targets:
            return create_error_response_json(
                "invalid target, valid: ["
                + " ".join([name for name, _ in targets.items()])
                + "]"
            )
        content_length_header = self.headers.get("Content-Length")
        if content_length_header is None:
            return create_error_response_json("Missing Content-Length header")
        try:
            content_len = int(content_length_header)
        except ValueError:
            return create_error_response_json("Invalid Content-Length header")
        body = self.rfile.read(content_len)
        return disassemble_wat(targets[target_name], body)


parser = argparse.ArgumentParser(description="Explorer Server")
parser.add_argument("--port", default=1235, type=int, help="port to listen on")
parser.add_argument(
    "--host",
    default="localhost",
    choices=["localhost", "0.0.0.0"],
    help="host to listen on (localhost or 0.0.0.0)",
)
parser.add_argument(
    "--log-level",
    default="WARNING",
    choices=["DEBUG", "INFO", "WARNING", "ERROR", "CRITICAL"],
    help="set the logging level",
)

args = parser.parse_args()

port = args.port
host = args.host

logging.basicConfig(
    level=args.log_level,
    format="[%(levelname)s] %(message)s",
)

httpd = HTTPServer((host, port), ExplorerServer)
print(f"explorer service open on http://{host}:{port}")
httpd.serve_forever()

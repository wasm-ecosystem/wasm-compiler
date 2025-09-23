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

from collections import defaultdict
from typing import List, Dict
from . import lib_dwarf, wasm_parser
import logging
import json
import subprocess


type MachineCodeOffset = int
type OpCode = int
type WatLineNumber = int
type AssemblyLineNumber = int
type WasmOpCodeStr = str
type WasmOpCodeOffset = int


def analyze_debug_info_in_dwarf(
    dwo: bytes,
    wasm: bytes,
    assembly: str,
):
    """
    from dwarf, we can get sequence of wasm op code(byte) and corresponding machine code offset
    from wat, we can parse the sequence of wasm op code(str) and corresponding line number
    we can match the two sequence to map machine code offset to line number.

    from dwarf from WARP, WasmOpCodeOffset -> MachineCodeOffset(*)                  [1]
    from dwarf in wasm -> WatLineNumber -> WasmOpCodeOffset(*)                      [4]
    from assembly, MachineCodeOffset -> AssemblyLineNumber(*)                       [5]
    from [4] + [1], WatLineNumber -> MachineCodeOffset(*)                           [6]
    from [5] + [6], WatLineNumber -> AssemblyLineNumber(*)
    """

    # [1]
    wasm_offset_to_machine_code_offset = decode_dwarf(dwo)
    logging.debug(
        f"wasm_offset_to_machine_code_offset: {wasm_offset_to_machine_code_offset}"
    )

    # [4]
    def parse_dwarf_in_wasm():
        code_section_offset = wasm_parser.get_code_section(wasm)
        results = defaultdict(list)
        mapping = lib_dwarf.get_wasm_wat_mapping(binary=wasm)
        mapping = json.loads(mapping)
        for item in mapping:
            results[item["line"]].append(item["address"] + code_section_offset)
        return dict(results)

    wat_line_to_wasm_offset = parse_dwarf_in_wasm()
    logging.debug(f"wat_line_to_wasm_offset: {wat_line_to_wasm_offset}")

    # [5]
    def decode_dis():
        machine_code_offset_to_disassembly_line: Dict[
            MachineCodeOffset, WatLineNumber
        ] = {}
        for i, line in enumerate(assembly.split("\n")):
            machine_code_offset_str = line.split(" ")[0]
            try:
                machine_code_offset = int(machine_code_offset_str, 16)
            except ValueError:
                continue
            machine_code_offset_to_disassembly_line[machine_code_offset] = i
        return machine_code_offset_to_disassembly_line

    machine_code_offset_to_disassembly_line = decode_dis()
    logging.debug(
        f"machine_code_offset_to_disassembly_line: {machine_code_offset_to_disassembly_line}"
    )

    wat_line_to_assembly_line: defaultdict[WatLineNumber, List[AssemblyLineNumber]] = (
        defaultdict(list)
    )

    for wat_line, wasm_offsets in wat_line_to_wasm_offset.items():
        logging.debug(f"wat_line: {wat_line}, wasm_offset: {wasm_offsets}")
        for wasm_offset in wasm_offsets:
            if wasm_offset not in wasm_offset_to_machine_code_offset:
                wasm_opcode_str = wasm_parser.get_wasm_op_code_str(wasm, wasm_offset)
                logging.info(
                    f"wasm_offset {wasm_offset}({wasm_opcode_str}) not found in wasm_offset_to_machine_code_offset"
                )
                continue
            machine_code_offsets = wasm_offset_to_machine_code_offset[wasm_offset]
            logging.debug(
                f"wat_line: {wat_line}, wasm_offset: {wasm_offset}, machine_code_offsets: {[hex(offset) for offset in machine_code_offsets]}"
            )
            for machine_code_offset in machine_code_offsets:
                assembly_line = machine_code_offset_to_disassembly_line[
                    machine_code_offset
                ]
                wat_line_to_assembly_line[wat_line].append(assembly_line)
    return dict(wat_line_to_assembly_line)


def decode_dwarf(dwo: bytes):
    result: defaultdict[WasmOpCodeOffset, List[MachineCodeOffset]] = defaultdict(list)
    debug_line: list[dict[str, int]] = json.loads(
        lib_dwarf.get_wasm_wat_mapping(binary=dwo)
    )
    for entry in debug_line:
        address = entry["address"]
        line = entry["line"]
        result[line].append(address)
    return dict(result)


def dump_dwo(dwo: bytes) -> str:
    cmd = ["llvm-dwarfdump", "-a", "-"]
    result = subprocess.run(cmd, input=dwo, check=True, stdout=subprocess.PIPE)
    return result.stdout.decode("utf-8")

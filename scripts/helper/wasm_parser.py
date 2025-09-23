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

import logging
from . import leb128
from typing import List, Tuple

CODE_SECTION_ID = 10


def get_code_section_impl(wasm: bytes) -> Tuple[int, int]:
    """
    Get the code section of a WebAssembly binary.

    Args:
        wasm (bytes): The WebAssembly binary data.

    Returns:
        int: The offset of the code section in the binary.
    """
    pos = 8
    while pos < len(wasm):
        section_id = wasm[pos]
        pos += 1
        (length, leb128_length) = leb128.u.decode(wasm[pos : pos + leb128.MAX_LENGTH])
        pos += leb128_length
        logging.debug(f"Section ID: {section_id}, Length: {length}, Position: {pos}")
        if section_id == CODE_SECTION_ID:
            (function_count, leb128_length) = leb128.u.decode(
                wasm[pos : pos + leb128.MAX_LENGTH]
            )
            pos += leb128_length
            return (pos, function_count)
        pos += length
    assert False


def get_code_section(wasm: bytes) -> int:
    return get_code_section_impl(wasm)[0]


def get_func_offsets(wasm: bytes) -> List[int]:
    results = []
    code_section_pos, function_count = get_code_section_impl(wasm)
    for _ in range(function_count):
        (function_body_size, leb128_length) = leb128.u.decode(
            wasm[code_section_pos : code_section_pos + leb128.MAX_LENGTH]
        )
        code_section_pos += leb128_length
        results.append(code_section_pos)
        code_section_pos += function_body_size
    return results


def get_wasm_op_code_str(wasm: bytes, offset: int) -> str:
    """
    Convert a wasm opcode to its string representation.
    """
    return wasm_opcode_to_str(
        wasm[offset]
        if wasm[offset] != 0xFC
        else int.from_bytes(wasm[offset : offset + 2], "big")
    )


def wasm_opcode_to_str(op: int) -> str:
    match (op):
        case 0x00:
            return "unreachable"
        case 0x01:
            return "nop"
        case 0x02:
            return "block"
        case 0x03:
            return "loop"
        case 0x04:
            return "if"
        case 0x05:
            return "else"
        case 0x0B:
            return "end"
        case 0x0C:
            return "br"
        case 0x0D:
            return "br_if"
        case 0x0E:
            return "br_table"
        case 0x0F:
            return "return"
        case 0x10:
            return "call"
        case 0x11:
            return "call_indirect"
        case 0x1A:
            return "drop"
        case 0x1B:
            return "select"
        case 0x20:
            return "local.get"
        case 0x21:
            return "local.set"
        case 0x22:
            return "local.tee"
        case 0x23:
            return "global.get"
        case 0x24:
            return "global.set"
        case 0x25:
            return "table.get"
        case 0x26:
            return "table.set"
        case 0x28:
            return "i32.load"
        case 0x29:
            return "i64.load"
        case 0x2A:
            return "f32.load"
        case 0x2B:
            return "f64.load"
        case 0x2C:
            return "i32.load8_s"
        case 0x2D:
            return "i32.load8_u"
        case 0x2E:
            return "i32.load16_s"
        case 0x2F:
            return "i32.load16_u"
        case 0x30:
            return "i64.load8_s"
        case 0x31:
            return "i64.load8_u"
        case 0x32:
            return "i64.load16_s"
        case 0x33:
            return "i64.load16_u"
        case 0x34:
            return "i64.load32_s"
        case 0x35:
            return "i64.load32_u"
        case 0x36:
            return "i32.store"
        case 0x37:
            return "i64.store"
        case 0x38:
            return "f32.store"
        case 0x39:
            return "f64.store"
        case 0x3A:
            return "i32.store8"
        case 0x3B:
            return "i32.store16"
        case 0x3C:
            return "i64.store8"
        case 0x3D:
            return "i64.store16"
        case 0x3E:
            return "i64.store32"
        case 0x3F:
            return "memory.size"
        case 0x40:
            return "memory.grow"
        case 0x41:
            return "i32.const"
        case 0x42:
            return "i64.const"
        case 0x43:
            return "f32.const"
        case 0x44:
            return "f64.const"
        case 0x45:
            return "i32.eqz"
        case 0x46:
            return "i32.eq"
        case 0x47:
            return "i32.ne"
        case 0x48:
            return "i32.lt_s"
        case 0x49:
            return "i32.lt_u"
        case 0x4A:
            return "i32.gt_s"
        case 0x4B:
            return "i32.gt_u"
        case 0x4C:
            return "i32.le_s"
        case 0x4D:
            return "i32.le_u"
        case 0x4E:
            return "i32.ge_s"
        case 0x4F:
            return "i32.ge_u"
        case 0x50:
            return "i64.eqz"
        case 0x51:
            return "i64.eq"
        case 0x52:
            return "i64.ne"
        case 0x53:
            return "i64.lt_s"
        case 0x54:
            return "i64.lt_u"
        case 0x55:
            return "i64.gt_s"
        case 0x56:
            return "i64.gt_u"
        case 0x57:
            return "i64.le_s"
        case 0x58:
            return "i64.le_u"
        case 0x59:
            return "i64.ge_s"
        case 0x5A:
            return "i64.ge_u"
        case 0x5B:
            return "f32.eq"
        case 0x5C:
            return "f32.ne"
        case 0x5D:
            return "f32.lt"
        case 0x5E:
            return "f32.gt"
        case 0x5F:
            return "f32.le"
        case 0x60:
            return "f32.ge"
        case 0x61:
            return "f64.eq"
        case 0x62:
            return "f64.ne"
        case 0x63:
            return "f64.lt"
        case 0x64:
            return "f64.gt"
        case 0x65:
            return "f64.le"
        case 0x66:
            return "f64.ge"
        case 0x67:
            return "i32.clz"
        case 0x68:
            return "i32.ctz"
        case 0x69:
            return "i32.popcnt"
        case 0x6A:
            return "i32.add"
        case 0x6B:
            return "i32.sub"
        case 0x6C:
            return "i32.mul"
        case 0x6D:
            return "i32.div_s"
        case 0x6E:
            return "i32.div_u"
        case 0x6F:
            return "i32.rem_s"
        case 0x70:
            return "i32.rem_u"
        case 0x71:
            return "i32.and"
        case 0x72:
            return "i32.or"
        case 0x73:
            return "i32.xor"
        case 0x74:
            return "i32.shl"
        case 0x75:
            return "i32.shr_s"
        case 0x76:
            return "i32.shr_u"
        case 0x77:
            return "i32.rotl"
        case 0x78:
            return "i32.rotr"
        case 0x79:
            return "i64.clz"
        case 0x7A:
            return "i64.ctz"
        case 0x7B:
            return "i64.popcnt"
        case 0x7C:
            return "i64.add"
        case 0x7D:
            return "i64.sub"
        case 0x7E:
            return "i64.mul"
        case 0x7F:
            return "i64.div_s"
        case 0x80:
            return "i64.div_u"
        case 0x81:
            return "i64.rem_s"
        case 0x82:
            return "i64.rem_u"
        case 0x83:
            return "i64.and"
        case 0x84:
            return "i64.or"
        case 0x85:
            return "i64.xor"
        case 0x86:
            return "i64.shl"
        case 0x87:
            return "i64.shr_s"
        case 0x88:
            return "i64.shr_u"
        case 0x89:
            return "i64.rotl"
        case 0x8A:
            return "i64.rotr"
        case 0x8B:
            return "f32.abs"
        case 0x8C:
            return "f32.neg"
        case 0x8D:
            return "f32.ceil"
        case 0x8E:
            return "f32.floor"
        case 0x8F:
            return "f32.trunc"
        case 0x90:
            return "f32.nearest"
        case 0x91:
            return "f32.sqrt"
        case 0x92:
            return "f32.add"
        case 0x93:
            return "f32.sub"
        case 0x94:
            return "f32.mul"
        case 0x95:
            return "f32.div"
        case 0x96:
            return "f32.min"
        case 0x97:
            return "f32.max"
        case 0x98:
            return "f32.copysign"
        case 0x99:
            return "f64.abs"
        case 0x9A:
            return "f64.neg"
        case 0x9B:
            return "f64.ceil"
        case 0x9C:
            return "f64.floor"
        case 0x9D:
            return "f64.trunc"
        case 0x9E:
            return "f64.nearest"
        case 0x9F:
            return "f64.sqrt"
        case 0xA0:
            return "f64.add"
        case 0xA1:
            return "f64.sub"
        case 0xA2:
            return "f64.mul"
        case 0xA3:
            return "f64.div"
        case 0xA4:
            return "f64.min"
        case 0xA5:
            return "f64.max"
        case 0xA6:
            return "f64.copysign"
        case 0xA7:
            return "i32.wrap_i64"
        case 0xA8:
            return "i32.trunc_f32_s"
        case 0xA9:
            return "i32.trunc_f32_u"
        case 0xAA:
            return "i32.trunc_f64_s"
        case 0xAB:
            return "i32.trunc_f64_u"
        case 0xAC:
            return "i64.extend_i32_s"
        case 0xAD:
            return "i64.extend_i32_u"
        case 0xAE:
            return "i64.trunc_f32_s"
        case 0xAF:
            return "i64.trunc_f32_u"
        case 0xB0:
            return "i64.trunc_f64_s"
        case 0xB1:
            return "i64.trunc_f64_u"
        case 0xB2:
            return "f32.convert_i32_s"
        case 0xB3:
            return "f32.convert_i32_u"
        case 0xB4:
            return "f32.convert_i64_s"
        case 0xB5:
            return "f32.convert_i64_u"
        case 0xB6:
            return "f32.demote_f64"
        case 0xB7:
            return "f64.convert_i32_s"
        case 0xB8:
            return "f64.convert_i32_u"
        case 0xB9:
            return "f64.convert_i64_s"
        case 0xBA:
            return "f64.convert_i64_u"
        case 0xBB:
            return "f64.promote_f32"
        case 0xBC:
            return "i32.reinterpret_f32"
        case 0xBD:
            return "i64.reinterpret_f64"
        case 0xBE:
            return "f32.reinterpret_i32"
        case 0xBF:
            return "f64.reinterpret_i64"
        case 0xC0:
            return "i32.extend8_s"
        case 0xC1:
            return "i32.extend16_s"
        case 0xC2:
            return "i64.extend8_s"
        case 0xC3:
            return "i64.extend16_s"
        case 0xC4:
            return "i64.extend32_s"
        case 0xD0:
            return "ref.null"
        case 0xD1:
            return "ref.is_null"
        case 0xD2:
            return "ref.func"
        case 0xFC08:
            return "memory.init"
        case 0xFC09:
            return "data.drop"
        case 0xFC0A:
            return "memory.copy"
        case 0xFC0B:
            return "memory.fill"
        case 0xFC0C:
            return "table.init"
        case 0xFC0D:
            return "element.drop"
        case 0xFC0E:
            return "table.copy"
        case 0xFC0F:
            return "table.grow"
        case 0xFC10:
            return "table.size"
        case 0xFC11:
            return "table.fill"
        case _:
            return f"unknown_opcode_{hex(op)}"

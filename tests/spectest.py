#! /usr/bin/python3
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

# -*- coding: UTF-8 -*-

import os
from os.path import join
import argparse
import json
import base64

root_path = os.path.abspath(join(os.path.dirname(__file__), ".."))
testcase_path = join(root_path, "tests", "testcases")
testcase_binary_folder = join(root_path, "tests", "testcases", "bin")
testcase_filepath = join(root_path, "tests", "testcases.json")
testsuite_path = None

BLACK_LIST = ["spectest_linking"]

wast2json_command = "wast2json -o %s %s"


def collect_proposals_testcases(root: str) -> list:
    return []


def collect_testcases(root: str) -> list:
    files = []
    for path in os.listdir(root):
        fullpath = join(root, path)
        if os.path.isdir(fullpath):
            if path == "proposals":
                files += collect_proposals_testcases(fullpath)
                continue
            files += collect_testcases(fullpath)
            continue
        if fullpath.endswith(".wast"):
            if "simd" in path:
                # tricore qemu do not have enough pflash, ignore all simd related case until we implement it.
                continue
            files.append(fullpath)
    return files


def get_testsuite_name(path):
    return os.path.relpath(path, testsuite_path).replace(os.path.sep, "_")[:-5]


def generate_json(testsuite_wast_filepaths, force: bool):
    testcases = {}
    for testsuite_wast_file in testsuite_wast_filepaths:
        testsuite_name = get_testsuite_name(testsuite_wast_file)
        if testsuite_name in BLACK_LIST:
            continue
        testsuite_json_file = join(testcase_path, testsuite_name + ".json")
        if not (os.path.exists(testsuite_json_file) and not force):
            os.system(wast2json_command % (testsuite_json_file, testsuite_wast_file))
        with open(testsuite_json_file, "r", encoding="UTF-8") as f:
            testsuite_json = json.load(f)
            short_filename = testsuite_name
            testcases[short_filename] = {
                "wast_json": testsuite_json,
            }
            for command in testsuite_json["commands"]:
                type = command["type"]
                if (
                    type == "module"
                    or type == "assert_invalid"
                    or type == "assert_malformed"
                ):
                    filename = command["filename"]
                    if filename[-4:] != "wasm":
                        continue
                    with open(
                        join(os.path.dirname(testsuite_json_file), filename), "rb"
                    ) as wasmfile:
                        wasm_binary = wasmfile.read()
                        wasm_b64 = str(base64.b64encode(wasm_binary), encoding="ascii")
                        testcases[short_filename][filename] = wasm_b64

    f = open(testcase_filepath, "w")
    json.dump(testcases, f)
    f.close()


def reorder_commands(commands):
    orderedCommandsName = ["none"]
    orderedCommands = [[]]
    ignores = []
    for command in commands:
        type = command["type"]
        if type == "module":
            if "name" in command:
                orderedCommandsName.append(command["name"])
            else:
                orderedCommandsName.append("!___def")
            orderedCommands.append([command])
        elif "action" in command and "module" in command["action"]:
            name = command["action"]["module"]
            if name in ignores:
                continue
            orderedCommands[orderedCommandsName.index(name)].append(command)
        elif "type" in command and "name" in command and command["type"] == "register":
            ignores.append(command["name"])
        else:
            orderedCommands[-1].append(command)
    return [i for item in orderedCommands for i in item]


def writeU8(buf: list, pos: int, value: int):
    while pos >= len(buf):
        buf.append(0)
    assert buf[pos] == 0
    assert value <= 255
    buf[pos] = value
    return pos + 1


def writeU16(buf: list, pos: int, value: int):
    writeU8(buf, pos, value >> 8)
    writeU8(buf, pos + 1, value & 0xFF)
    return pos + 2


def writeU32(buf: list, pos: int, value: int):
    writeU16(buf, pos, value >> 16)
    writeU16(buf, pos + 2, value & 0xFFFF)
    return pos + 4


def writeSpan(buf: list, pos: int, value: list[int]):
    pos = writeU32(buf, pos, len(value))
    for v in value:
        pos = writeU8(buf, pos, v)
    return pos


def writeString(buf: list, pos: int, value: str):
    encodedValue = str.encode(value, encoding="utf-8")
    pos = writeU32(buf, pos, len(encodedValue))
    for char in encodedValue:
        pos = writeU8(buf, pos, char)
    return pos


def generate_binary(testsuite_wast_filepaths: list[str], force: bool) -> list[str]:
    testsuite_binary_filepaths: list[str] = []
    for testsuite_wast_file in testsuite_wast_filepaths:
        testsuite_name = get_testsuite_name(testsuite_wast_file)
        if testsuite_name in BLACK_LIST:
            continue
        testsuite_json_file = join(testcase_path, testsuite_name + ".json")
        if not (os.path.exists(testsuite_json_file) and not force):
            os.system(wast2json_command % (testsuite_json_file, testsuite_wast_file))
        with open(testsuite_json_file, "r", encoding="UTF-8") as f:
            binary_file_path = join(testcase_binary_folder, testsuite_name + ".bin")
            binary_file = open(binary_file_path, "wb")
            testsuite_binary_filepaths.append(binary_file_path)

            testsuite_json = json.load(f)
            commands = reorder_commands(commands=testsuite_json["commands"])
            buffer = []
            pos = 0
            pos = writeString(buffer, pos, testsuite_name)
            has_module = True
            for command in commands:
                # guard
                type = command["type"]
                if (
                    type == "assert_uninstantiable"
                    or type == "assert_unlinkable"
                    or type == "register"
                ):
                    continue
                if (
                    type == "module"
                    or type == "assert_invalid"
                    or type == "assert_malformed"
                ):
                    filename = command["filename"]
                    if filename[-4:] != "wasm":
                        has_module = False
                        continue
                    has_module = True
                if not has_module:
                    continue
                # encode, detail see cpp
                lengthPos = pos
                pos = writeU32(buffer, pos, 0)  # commandLength
                if command["type"] == "module":
                    pos = writeU8(buffer, pos, 0)  # type
                elif command["type"] == "assert_return":
                    pos = writeU8(buffer, pos, 1)  # type
                elif command["type"] == "action":
                    pos = writeU8(buffer, pos, 2)  # type
                elif command["type"] == "assert_trap":
                    pos = writeU8(buffer, pos, 3)  # type
                elif command["type"] == "assert_exhaustion":
                    pos = writeU8(buffer, pos, 4)  # type
                elif command["type"] == "assert_invalid":
                    pos = writeU8(buffer, pos, 5)  # type
                elif command["type"] == "assert_malformed":
                    pos = writeU8(buffer, pos, 9)  # type
                pos = writeU32(buffer, pos, command["line"])  # line

                if (
                    type == "module"
                    or type == "assert_invalid"
                    or type == "assert_malformed"
                ):
                    filename = command["filename"]
                    assert filename[-4:] == "wasm"
                    wasmfile = open(
                        join(os.path.dirname(testsuite_json_file), filename), "rb"
                    )
                    wasm_binary = wasmfile.read()

                    pos = writeSpan(buffer, pos, list(wasm_binary))  # bytecode

                elif (
                    type == "assert_return"
                    or type == "action"
                    or type == "assert_trap"
                    or type == "assert_exhaustion"
                ):
                    actionLengthPos = pos
                    pos = writeU32(buffer, pos, 0)  # actionLength
                    if command["action"]["type"] == "get":
                        pos = writeU8(buffer, pos, 0)
                    elif command["action"]["type"] == "invoke":
                        pos = writeU8(buffer, pos, 1)
                    pos = writeString(buffer, pos, command["action"]["field"])

                    if "args" in command["action"]:
                        args = command["action"]["args"]
                    else:
                        args = []
                    pos = writeU32(buffer, pos, len(args))
                    for arg in args:
                        pos = writeString(buffer, pos, arg["type"])
                        if "value" in arg:
                            if isinstance(arg["value"], list):  # SIMD
                                continue
                            pos = writeString(buffer, pos, arg["value"])
                        else:
                            pos = writeString(buffer, pos, "0")

                    actionLength = pos - actionLengthPos - 4
                    writeU32(buffer, actionLengthPos, actionLength)

                    pos = writeU32(buffer, pos, len(command["expected"]))
                    for arg in command["expected"]:
                        pos = writeString(buffer, pos, arg["type"])
                        if "value" in arg:
                            if isinstance(arg["value"], list):  # SIMD
                                continue
                            pos = writeString(buffer, pos, arg["value"])
                        else:
                            pos = writeString(buffer, pos, "0")

                    if "text" in command:
                        pos = writeString(buffer, pos, command["text"])
                    else:
                        pos = writeString(buffer, pos, "")

                length = pos - lengthPos - 4
                assert length != 0
                writeU32(buffer, lengthPos, length)  # fill commandLength

            writeU32(buffer, pos, 0)  # stop

            binary_file.write(bytes(buffer))
            binary_file.close()
    return testsuite_binary_filepaths


def assembly_binary(binary_filepaths: list[str]):
    payloads = [[]]
    for binary_filePath in binary_filepaths:
        with open(binary_filePath, "rb") as f:
            payload = f.read()
            payloads[-1] += list(payload)
            if len(payloads[-1]) > 1.5 * 1024 * 1024:  # keep one file less than 1.5MB
                payloads.append([])

    for i in range(len(payloads)):
        payload = payloads[i]
        total_c = open(join(testcase_binary_folder, f"total_{i}.cpp"), "w")
        total_c.write(
            """
    // clang-format off
    #include <array>
    #include <cstdint>
    #include <cstdlib>

    std::array<uint8_t, {0}U> constexpr const testcases = {{{1}}};
    void const * pTestcase = testcases.data();
    size_t testcaseSize = testcases.size();
    \n""".format(
                len(payload), ",".join([hex(e) for e in payload])
            )
        )
        print(f"generate file in total_{i}.cpp")
    assert len(payloads) <= 2  # we only have 2 standalone test case in cmake and bazel


def generate_testcases(force: bool):
    if not os.path.exists(testcase_path):
        os.mkdir(testcase_path)
    if not os.path.exists(testcase_binary_folder):
        os.mkdir(testcase_binary_folder)
    testsuite_wast_filepaths = collect_testcases(testsuite_path)
    generate_json(testsuite_wast_filepaths, force)
    binary_filepaths = generate_binary(testsuite_wast_filepaths, force)
    assembly_binary(binary_filepaths)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="spectest case generator")
    parser.add_argument(
        "--force-generate", "-f", action="store_true", help="force update testcase"
    )
    parser.add_argument(
        "--testsuite-path",
        default=join(root_path, "tests", "testsuite"),
        help="path of testsuite",
    )

    args = parser.parse_args()
    testsuite_path = args.testsuite_path

    generate_testcases(args.force_generate)

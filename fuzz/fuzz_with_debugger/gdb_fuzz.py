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

import sys  # nopep8

print(f"Python version: {sys.version}")

import gdb
import os  # nopep8

sys.path.append(os.path.dirname(os.path.realpath(__file__)))  # nopep8
from dbg_fuzz import DbgFuzz  # nopep8


targetDir = os.getenv("VB_FUZZ_TARGET_DIR", ".")
execPrefix = os.getenv("VB_FUZZ_EXEC_PREFIX", "")
fuzzOffset = os.getenv("VB_FUZZ_OFFSET", "0")
nativeBuildFolder = os.getenv("VB_FUZZ_NATIVE_BUILD_FOLDER", "/")

generateBinaryAndBackupFailedModules = True
compileNatively = False


class GDBFuzz(DbgFuzz, gdb.Breakpoint):

    def __init__(
        self,
        targetDir: str,
        execPrefix: str,
        nativeBuildFolder: str,
        fuzzOffset: str,
        compileNatively: bool,
    ) -> None:
        DbgFuzz.__init__(
            self, targetDir, execPrefix, nativeBuildFolder, fuzzOffset, compileNatively
        )
        gdb.Breakpoint.__init__(self, "GDB_FUZZ_UPDATE")

    def stop(self) -> bool:
        return self.processBreakPoint()

    def getIntByVariableName(self, name: str) -> int:
        return int(gdb.parse_and_eval(name))

    def getAddressByVariableName(self, name: str) -> int:
        return int(gdb.parse_and_eval(f"&{name}"))

    def getBytesByVariableName(self, variableName: str, size: int) -> str:
        address = self.getAddressByVariableName(variableName)

        memoryView = gdb.inferiors()[0].read_memory(address, size)
        message = memoryView.tobytes()
        return message

    def setVariableInt(self, variableName: str, value: int) -> None:
        self.__setVariableByName(variableName, str(value))

    def setVariableBool(self, variableName: str, value: bool) -> None:
        self.__setVariableByName(variableName, "true" if value else "false")

    def setMemoryByAddress(self, address: int, data: bytes) -> None:
        gdb.inferiors()[0].write_memory(address, data, len(data))

    def __setVariableByName(self, variableName: str, value: str) -> None:
        gdb.execute(f"set var {variableName} = {value}")


gdb.execute("set pagination off")
gdb.execute("set confirm off")
gdb.execute("set verbose off")

GDBFuzz(targetDir, execPrefix, nativeBuildFolder, fuzzOffset, compileNatively)

print("Fuzzing in " + targetDir)

gdb.execute("continue")
gdb.execute("quit")

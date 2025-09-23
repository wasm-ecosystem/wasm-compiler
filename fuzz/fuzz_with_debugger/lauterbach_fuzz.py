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

from dbg_fuzz import DbgFuzz
import lauterbach.trace32.rcl as t32
from lauterbach.trace32.rcl._rc._symbol import Symbol
import os


targetDir = os.getenv("VB_FUZZ_TARGET_DIR", ".")
execPrefix = os.getenv("VB_FUZZ_EXEC_PREFIX", "")
fuzzOffset = os.getenv("VB_FUZZ_OFFSET", "0")
nativeBuildFolder = os.getenv("VB_FUZZ_NATIVE_BUILD_FOLDER", "/")

generateBinaryAndBackupFailedModules = True
compileNatively = False


class LauterbachFuzz(DbgFuzz):

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
        self.__symbolCache: dict[str, Symbol] = {}
        self.__dbg = t32.connect(
            node="localhost", port=20000, protocol="TCP", timeout=3.0
        )
        self.__flashFuzzElf()
        self.__initBreakpoint()

    def getIntByVariableName(self, name: str) -> int:
        symbol: Symbol = self.__getSymbolByVariableName(name)
        value: int = 0
        if symbol.size == 1:
            value = self.__dbg.memory.read_uint8(symbol.address)
        elif symbol.size == 2:
            value = self.__dbg.memory.read_uint16(symbol.address)
        elif symbol.size == 4:
            value = self.__dbg.memory.read_uint32(symbol.address)
        else:
            raise Exception("invalid symbol size")
        return value

    def getAddressByVariableName(self, name: str) -> int:
        symbol = self.__getSymbolByVariableName(name)
        return symbol.address.value

    def getBytesByVariableName(self, variableName: str, size: int) -> bytes:
        symbol = self.__getSymbolByVariableName(variableName)
        memoryData = self.__dbg.memory.read_uint8_array(
            address=symbol.address, length=size, width=1
        )

        return memoryData.tobytes()

    def setVariableInt(self, variableName: str, value: int) -> None:
        symbol = self.__getSymbolByVariableName(variableName)
        if symbol.size == 1:
            value = self.__dbg.memory.write_uint8(symbol.address, value)
        elif symbol.size == 2:
            value = self.__dbg.memory.write_uint16(symbol.address, value)
        elif symbol.size == 4:
            value = self.__dbg.memory.write_uint32(symbol.address, value)
        else:
            raise Exception("invalid symbol size")

    def setVariableBool(self, variableName: str, value: bool) -> None:
        self.setVariableInt(variableName, int(value))

    def setMemoryByAddress(self, addressInt: int, data: bytes) -> None:
        address = self.__dbg.address(access="D", value=addressInt)
        int_tuple = tuple(int(byte) for byte in data)
        self.__dbg.memory.write_uint8_array(address=address, data=int_tuple)

    def startFuzz(self) -> None:
        while True:
            state = self.__dbg.get_state()
            if len(state) > 0:
                stateValue = state[0]

                if stateValue == 2:  # break point hit
                    self.processBreakPoint()
                    self.__dbg.go()

    def __getSymbolByVariableName(self, variableName: str) -> Symbol:
        symbol = self.__symbolCache.get(variableName)

        if symbol == None:
            symbol = self.__dbg.symbol.query_by_name(variableName)
            self.__symbolCache[variableName] = symbol
        return symbol

    def __flashFuzzElf(self) -> None:
        projectRootPath = os.path.dirname(
            os.path.dirname(os.path.dirname(os.path.realpath(__file__)))
        )
        cmmFilePath = os.path.join(projectRootPath, "cmm", "fuzz_flash.cmm")
        self.__dbg.cmm(cmmFilePath)

    def __initBreakpoint(self) -> None:
        symbol = self.__getSymbolByVariableName("GDB_FUZZ_UPDATE")
        self.__breakpoint = self.__dbg.breakpoint.set(address=symbol.address)

    # Due to limited memory on real mcu, the linear memory extend failure can't be avoided
    def errorMessageFiler(self, message: str) -> bool:
        if message.startswith("Could not extend linear memory"):
            return False
        return True

    def disconnect(self):
        self.__dbg.disconnect()


print("Fuzzing in " + targetDir)

lauterbachFuzz = LauterbachFuzz(
    targetDir, execPrefix, nativeBuildFolder, fuzzOffset, compileNatively
)

lauterbachFuzz.startFuzz()

lauterbachFuzz.disconnect()

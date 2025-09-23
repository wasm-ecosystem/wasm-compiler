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

from abc import abstractmethod
from fuzz_module_manager import FuzzModuleManager
import datetime
import re
import struct
import logging
import os
import sys


class ConsoleHandler(logging.StreamHandler):
    def emit(self, record):
        msg = self.format(record)
        print(msg)


class DbgFuzz:
    def __init__(
        self,
        targetDir: str,
        execPrefix: str,
        nativeBuildFolder: str,
        fuzzOffset: str,
        compileNatively: bool,
    ) -> None:
        self.count = 0
        self.oldFunctionsExecuted = 0

        self.timeBinaries = 0
        self.timeReference = 0
        self.__compileNatively = compileNatively
        self.__fuzzModuleManager = FuzzModuleManager(
            targetDir, execPrefix, nativeBuildFolder, fuzzOffset
        )
        self.__failedExecutions = 0

        logLevel = os.getenv("VB_FUZZ_LOGGING_LEVEL", "")

        if logLevel.lower() == "debug":
            logging.basicConfig(level=logging.DEBUG)

        self.__logger = logging.getLogger()
        handler = ConsoleHandler(stream=sys.stdout)
        self.__logger.addHandler(handler)

    def processBreakPoint(self) -> bool:
        if self.count == 0:
            self.startTime = datetime.datetime.now()
            self.oldTime = datetime.datetime.now()
        else:
            self.__checkLastError()
            if self.count % 20 == 0:
                functionsExecuted = self.getIntByVariableName("functionsExecuted")

                newTime = datetime.datetime.now()
                totalTime = (newTime - self.startTime).total_seconds()
                output = (
                    f"{functionsExecuted} function calls ({self.count} modules) executed in {round((newTime - self.startTime).total_seconds(), 1)} s ({self.__failedExecutions} failed) - {round((functionsExecuted - self.oldFunctionsExecuted)/((newTime - self.oldTime).total_seconds()), 1)} f/s (last 10 modules), {round(functionsExecuted/((newTime - self.startTime).total_seconds()), 1)} f/s (all)\n"
                    f"{round(100 * self.timeBinaries/totalTime, 1)}% of time spent generating binaries, {round(100 * self.timeReference/totalTime, 1)}% for executing reference interpreter, {round(100 * (1 - (self.timeBinaries + self.timeReference)/totalTime), 1)}% for VB execution\n"
                )
                self.oldFunctionsExecuted = functionsExecuted
                self.oldTime = newTime

                self.__fuzzModuleManager.writeStatus(output)

        self.setVariableBool("VBHELPER_GDB_FUZZ_ITERATION_FAILED", False)

        buf_adr = self.getAddressByVariableName("VBHELPER_GDB_FUZZ_INPUT_BINARY")
        cur_buf_len = self.getIntByVariableName(
            "VBHELPER_GDB_FUZZ_INPUT_BINARY_ACTUAL_LENGTH"
        )

        self.time1 = datetime.datetime.now()

        self.__fuzzModuleManager.generateWasmBinary()

        new_binary = ""
        if self.__compileNatively:
            self.setVariableBool("VBHELPER_INPUT_IS_ALREADY_COMPILED", True)
            new_binary = self.__fuzzModuleManager.generateAndLoadNativeBinary()
        else:
            self.setVariableBool("VBHELPER_INPUT_IS_ALREADY_COMPILED", False)
            new_binary = self.__fuzzModuleManager.loadWasmBinary()

        self.time2 = datetime.datetime.now()
        ref_out = self.__fuzzModuleManager.generateReferenceOutput()
        self.time3 = datetime.datetime.now()

        self.timeBinaries = (
            self.timeBinaries + (self.time2 - self.time1).total_seconds()
        )
        self.timeReference = (
            self.timeReference + (self.time3 - self.time2).total_seconds()
        )

        self.setMemoryByAddress(buf_adr, ref_out)
        self.setMemoryByAddress(buf_adr + len(ref_out), new_binary)
        self.setVariableInt("VBHELPER_GDB_FUZZ_INPUT_REFOUTPUT_LENGTH", len(ref_out))
        self.setVariableInt(
            "VBHELPER_GDB_FUZZ_INPUT_BINARY_ACTUAL_LENGTH",
            len(ref_out) + len(new_binary),
        )

        self.count = self.count + 1
        return False

    def __checkLastError(self) -> None:
        last_iteration_failed = self.getIntByVariableName(
            "VBHELPER_GDB_FUZZ_ITERATION_FAILED"
        )

        lastExecutionWithError = last_iteration_failed != 0
        errorMessageList = []
        if not lastExecutionWithError:
            refStr = self.__fuzzModuleManager.getRefLast().decode("ascii")
            refList = refStr.split("\n")
            formatStringList = ["<"]

            self.__logger.debug("reference string is:")
            self.__logger.debug(refStr)

            refNumbers = []

            for refLine in refList:
                pattern = r"\b(i32|i64|f32|f64):([-+]?\b(?:\d+(?:\.\d+)?|inf))\b"
                matches = re.findall(pattern, refLine)

                for match in matches:
                    prefix = match[0]
                    number = match[1]

                    refNumbers.append(number)

                    if prefix == "i32":
                        formatStringList.append("I")
                    elif prefix == "i64":
                        formatStringList.append("Q")
                    elif prefix == "f32":
                        formatStringList.append("f")
                    elif prefix == "f64":
                        formatStringList.append("d")
                    else:
                        raise "Unknown prefix"

            unpackStr = "".join(formatStringList)

            outputResultBytesLength = self.getIntByVariableName(
                "VBHELPER_GDB_FUZZ_OUTPUT_RESULT_LENGTH"
            )

            if outputResultBytesLength > 0:
                self.__logger.debug(unpackStr)
                self.__logger.debug(str(outputResultBytesLength))

                assert (
                    struct.calcsize(unpackStr) == outputResultBytesLength
                ), "unpack string mismatch result size"

                self.__logger.debug(str(outputResultBytesLength))
                outputResultBytes = self.getBytesByVariableName(
                    "VBHELPER_GDB_FUZZ_OUTPUT_RESULT", outputResultBytesLength
                )

                self.__logger.debug(outputResultBytes)
                self.__logger.debug(unpackStr)

                outputResultList = struct.unpack(unpackStr, outputResultBytes)

                self.__logger.debug(str(len(outputResultList)))

                assert len(outputResultList) == len(
                    refNumbers
                ), "number of result mismatch with reference"

                for i in range(0, len(outputResultList)):
                    resultStr = DbgFuzz.__formatNumber(outputResultList[i])
                    refStr = refNumbers[i]
                    if resultStr != refStr:
                        lastExecutionWithError = True
                        errorMessageList.append(
                            f"wrong result actual {resultStr} vs. expected {refStr}"
                        )

        if lastExecutionWithError:
            print("ITERATION FAILED")
            messageSize = self.getIntByVariableName(
                "VBHELPER_GDB_FUZZ_OUTPUT_MESSAGE_SIZE"
            )
            failedMessage = self.getBytesByVariableName(
                "VBHELPER_GDB_FUZZ_OUTPUT_MESSAGE", messageSize
            ).decode("ascii")
            errorMessageList.append(failedMessage)
            errorMessage = "\n".join(errorMessageList)
            if self.errorMessageFiler(errorMessage):
                self.__failedExecutions += 1
                self.__fuzzModuleManager.saveLastModule(errorMessage)

    @staticmethod
    def __formatNumber(number, precision=6) -> str:
        """Formats a number to string, keeping the specified precision."""
        if isinstance(number, float):
            return format(number, ".{}f".format(precision))
        else:
            return str(number)

    @abstractmethod
    def getIntByVariableName(self, name: str) -> int:
        pass

    @abstractmethod
    def getAddressByVariableName(self, name: str) -> int:
        pass

    @abstractmethod
    def getBytesByVariableName(self, variableName: str, size: int) -> bytes:
        pass

    @abstractmethod
    def setVariableInt(self, variableName: str, value: int) -> None:
        pass

    @abstractmethod
    def setVariableBool(self, variableName: str, value: bool) -> None:
        pass

    @abstractmethod
    def setMemoryByAddress(self, address: int, data: bytes) -> None:
        pass

    # suppress some testing errors if this function returns false
    def errorMessageFiler(self, message: str) -> bool:
        return True

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

import string
import subprocess
import random
import os


class FuzzModuleManager:
    def __init__(
        self, targetDir: str, execPrefix: str, nativeBuildFolder: str, fuzzOffset: str
    ):
        self.generateBinaryAndBackupFailedModules = True
        self.__targetDir = targetDir
        self.__execPrefix = execPrefix
        self.__nativeBuildFolder = nativeBuildFolder
        self.__fuzzOffset = fuzzOffset
        self.__targetWasmPath = os.path.join(self.__targetDir, "new.wasm")
        self.__nativeBinaryPath = os.path.join(self.__targetDir, "out.bin")
        self.__seed_file_name = os.path.join(self.__targetDir, "seed.txt")
        self.__status_file_path = os.path.join(self.__targetDir, "status.txt")
        self.__vb_simple_compile_path = os.path.join(
            self.__nativeBuildFolder, "bin", "vb_simple_compile"
        )

        self.__failedModuleFolder = os.path.join(self.__targetDir, "failedmodules")
        if not os.path.exists(self.__failedModuleFolder):
            os.makedirs(self.__failedModuleFolder)

        if not os.path.exists(self.__targetDir):
            os.makedirs(self.__targetDir)

        self.__init_random()

    def __generate_random_string(self, length: int) -> str:
        letters = string.ascii_letters + string.digits + string.punctuation
        return "".join(random.choice(letters) for _ in range(length))

    def __write_string_to_file(self, file_name: str, content: str) -> None:
        with open(file_name, "w") as file:
            file.write(content)

    def __generateSeedFile(self, output_file_name: str) -> None:
        seed = self.__generate_random_string(200)
        self.__write_string_to_file(output_file_name, seed)

    def generateWasmBinary(self) -> None:
        self.__generateSeedFile(self.__seed_file_name)
        if self.generateBinaryAndBackupFailedModules:
            retCode = subprocess.call(
                [
                    self.__execPrefix + "wasm-opt",
                    self.__seed_file_name,
                    "-ttf",
                    "--enable-multivalue",
                    "--enable-bulk-memory-opt",
                    "--denan",
                    "-O2",
                    "-o",
                    self.__targetWasmPath,
                ],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
            )
            if retCode != 0:
                print("wasm-opt failed")
                exit(1)

    def __generateCompiledBinaryNatively(self) -> None:

        retCode = subprocess.call(
            [
                self.__vb_simple_compile_path,
                self.__targetWasmPath,
                self.__nativeBinaryPath,
            ]
        )
        if retCode != 0:
            print("vb_simple_compile failed")
            exit(1)

    def loadWasmBinary(self) -> bytes:
        with open(self.__targetWasmPath, mode="rb") as file:
            return file.read()

    def generateAndLoadNativeBinary(self) -> None:
        self.__generateCompiledBinaryNatively()
        with open(self.__nativeBinaryPath, mode="rb") as file:
            return file.read()

    def generateReferenceOutput(self) -> bytes:
        refOutBytes = subprocess.check_output(
            [
                self.__execPrefix + "wasm-interp",
                "--run-all-exports",
                "--dummy-import-func",
                self.__targetWasmPath,
            ]
        )
        # Normalize nan
        self.refLast = refOutBytes.replace(b"-nan", b"nan")
        return self.refLast

    def getRefLast(self) -> bytes:
        return self.refLast

    def writeStatus(self, content: str) -> None:
        print(content)
        self.__write_string_to_file(self.__status_file_path, content)

    def __init_random(self) -> None:
        seed_offset = 0
        try:
            seed_offset = int(self.__fuzzOffset)
        except ValueError:
            print("Seed offset couldn't be parsed")
            seed_offset = 0

        print("Generating nth iteration as seed: " + str(seed_offset))

        random.seed()
        seed = random.random()
        for i in range(seed_offset):
            seed = random.random()

        random.seed(seed)

    def saveLastModule(self, message: str) -> None:
        i = 1

        failedModulePath = ""
        while True:
            failedModulePath = os.path.join(self.__failedModuleFolder, f"new_{i}.wasm")
            if not os.path.exists(failedModulePath):
                break
            else:
                i += 1

        if self.generateBinaryAndBackupFailedModules:
            os.rename(self.__targetWasmPath, failedModulePath)
            self.__write_string_to_file(
                os.path.join(self.__failedModuleFolder, f"new_{i}_error_message.txt"),
                message,
            )

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

# tricore qemu has a bug that it exit with 0 in some crash, this python wrapper to check the output complete, otherwise exit with 1

import subprocess
import sys

expected_last_line = "0 tests failed total"


def run_command_and_check_last_line(command):
    try:
        process = subprocess.Popen(
            command,
            shell=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
        )

        last_line = None
        while True:
            output = process.stdout.readline()
            if output == "" and process.poll() is not None:
                break
            if output:
                last_line = output.strip()
                print(last_line)

        return_code = process.wait()

        error = process.stderr.read()
        if error:
            print(f"Error occurred: {error}")
            exit(1 if return_code == 0 else return_code)

        if return_code == 0 and (not last_line or expected_last_line not in last_line):
            exit(1)

        exit(return_code)

    except subprocess.CalledProcessError as e:
        print(f"Command failed with error: {e}")
        exit(e.returncode)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        print("Usage: python script.py <command>")
        sys.exit(1)

    command = " ".join(sys.argv[1:])
    run_command_and_check_last_line(command)

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

import sys
import os
import subprocess


cctc_args = ["cctc"]


def proceedArg(arg):
    if arg == "-MD" or arg == "-MF" or arg.startswith("-DBAZEL_CURRENT_REPOSITORY"):
        pass
    elif arg.endswith(".d"):
        file = open(arg, "a")
        file.close()
    elif arg == "-iquote" or arg == "-isystem":
        cctc_args.append("-I")
    else:
        cctc_args.append(arg)


def read_file_as_arguments(file_path):
    argument_file = open(file_path)

    argument_lines = argument_file.readlines()

    argument_file.close()

    for arg in argument_lines:
        if arg.startswith("-Wl,-S"):
            continue
        else:
            proceedArg(arg[:-1])


for i in range(1, len(sys.argv)):
    arg = sys.argv[i]

    if arg.startswith("@"):
        read_file_as_arguments(arg[1:])
    else:
        proceedArg(arg)


subprocess.run(
    cctc_args, env=os.environ, stdout=sys.stdout, stderr=sys.stderr, check=True
)

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


def append_artc_argument(artc_args, arg):
    if arg == "rcsD":
        artc_args.append("-r")
    else:
        artc_args.append(arg)


def read_file_as_arguments(artc_args, file_path):
    argument_file = open(file_path)

    argument_lines = argument_file.readlines()

    argument_file.close()

    for arg in argument_lines:
        append_artc_argument(artc_args, arg[:-1])


artc_args = ["artc"]

for i in range(1, len(sys.argv)):
    arg = sys.argv[i]
    if arg.startswith("@"):
        read_file_as_arguments(artc_args, arg[1:])
    else:
        append_artc_argument(artc_args, arg)

subprocess.run(
    artc_args, env=os.environ, stdout=sys.stdout, stderr=sys.stderr, check=True
)

#!/usr/bin/python3
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

"""
This is a replacement version of ./qcc bash in python, just for backup
"""
import sys
import os
import wrapper_command

args = sys.argv[1:]

if "-o" in args:
    o_index = args.index("-o")

    out_file = args[o_index + 1]

    d_file = os.path.splitext(out_file)[0] + ".d"

    f = open(d_file, "w+")
    f.close()

wrapper_command.run_wrapper_command("qcc", args)

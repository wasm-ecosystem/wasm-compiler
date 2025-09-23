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

import os
import subprocess


def run_wrapper_command(exec_name, args):
    qcc_env = os.environ.copy()
    if "QNX_HOST" in qcc_env:
        args.insert(0, "{}/usr/bin/{}".format(qcc_env["QNX_HOST"], exec_name))
    else:
        print("QNX_HOST is not set")
        exit(1)
    qcc_cwd = os.getcwd()

    popen = subprocess.Popen(args, env=qcc_env, cwd=qcc_cwd, stdout=subprocess.PIPE)

    return_code = popen.wait()

    if return_code != 0:
        print("{} failed".format(exec_name))
    exit(return_code)

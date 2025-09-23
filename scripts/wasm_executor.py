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

import argparse
import vb_warp
from helper import wasm_utils

parser = argparse.ArgumentParser(description="run wasm module")
parser.add_argument("--module", help="path to the wasm file", required=True)
parser.add_argument("--target", help="target function to execute", required=True)
parser.add_argument("--signature", help="function signature", required=True)
parser.add_argument("args", nargs="*", help="Arguments")

args = parser.parse_args()

compile = vb_warp.Compiler()
module = compile.compile(wasm_utils.load_wasm_or_wat(args.module))

runtime = vb_warp.Runtime()
runtime.load(module)
runtime.start()


target: str = str(args.target)
signature: str = str(args.signature)
arguments = []

for c in signature:
    if c == "(":
        continue
    if c == ")":
        if not len(args.args) == 0:
            raise Exception("Too many arguments")
        break
    if len(args.args) == 0:
        raise Exception("Too less arguments")
    match c:
        case "i":
            arguments.append(vb_warp.i32(int(args.args.pop(0))))
        case "I":
            arguments.append(vb_warp.i64(int(args.args.pop(0))))
        case "f":
            arguments.append(vb_warp.f32(float(args.args.pop(0))))
        case "F":
            arguments.append(vb_warp.f64(float(args.args.pop(0))))


res = runtime.call(target, signature, arguments)

print(res)

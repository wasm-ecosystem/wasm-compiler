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

set -e

VERSION="1.235.0" 

if [ -f "wasm-tools-$VERSION-x86_64-linux.tar.gz" ]; then
  rm ./wasm-tools-$VERSION-x86_64-linux.tar.gz
fi
wget https://github.com/bytecodealliance/wasm-tools/releases/download/v$VERSION/wasm-tools-$VERSION-x86_64-linux.tar.gz

if [ -f "wasm-tools-$VERSION-x86_64-linux/wasm-tools" ]; then
  rm ./wasm-tools-$VERSION-x86_64-linux/wasm-tools
fi
tar -zxvf wasm-tools-$VERSION-x86_64-linux.tar.gz wasm-tools-$VERSION-x86_64-linux/wasm-tools -C wasm-tools11
mv ./wasm-tools-$VERSION-x86_64-linux ./wasm-tools

rm ./wasm-tools-$VERSION-x86_64-linux.tar.gz

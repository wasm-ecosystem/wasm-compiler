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

if [ "$(uname -s)" = "Darwin" ] && [ "$(uname -m)" = "arm64" ]; then
  PLATFORM="aarch64-macos"
elif [ "$(uname -s)" = "Linux" ] && [ "$(uname -m)" = "x86_64" ]; then
  PLATFORM="x86_64-linux"
else
  echo "Unsupported platform: $(uname -s) $(uname -m)"
  exit 1
fi

ARCHIVE="wasm-tools-$VERSION-$PLATFORM.tar.gz"
EXTRACT_DIR="wasm-tools-$VERSION-$PLATFORM"

if [ -f "$ARCHIVE" ]; then
  rm ./$ARCHIVE
fi
wget https://github.com/bytecodealliance/wasm-tools/releases/download/v$VERSION/$ARCHIVE

if [ -f "$EXTRACT_DIR/wasm-tools" ]; then
  rm ./$EXTRACT_DIR/wasm-tools
fi
tar -zxvf $ARCHIVE $EXTRACT_DIR/wasm-tools
mv ./$EXTRACT_DIR ./wasm-tools

rm ./$ARCHIVE

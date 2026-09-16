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

VERSION="1.0.42"

if [ "$(uname -s)" = "Darwin" ] && [ "$(uname -m)" = "arm64" ]; then
  PLATFORM="macos-arm64"
elif [ "$(uname -s)" = "Linux" ] && [ "$(uname -m)" = "x86_64" ]; then
  PLATFORM="linux-x64"
elif [ "$(uname -s)" = "Linux" ] && ([ "$(uname -m)" = "aarch64" ] || [ "$(uname -m)" = "arm64" ]); then
  PLATFORM="linux-arm64"
elif case "$(uname -s)" in MINGW*|MSYS*|CYGWIN*|Windows_NT*) true ;; *) false ;; esac; then
  PLATFORM="windows-x64"
else
  echo "Unsupported platform: $(uname -s) $(uname -m)"
  exit 1
fi

ARCHIVE="wabt-$VERSION-$PLATFORM.tar.gz"
EXTRACT_DIR="wabt-$VERSION"

if [ -f "$ARCHIVE" ]; then
  rm -f "./$ARCHIVE"
fi

if command -v wget >/dev/null 2>&1; then
  wget "https://github.com/WebAssembly/wabt/releases/download/$VERSION/$ARCHIVE"
elif command -v curl >/dev/null 2>&1; then
  curl -L -O "https://github.com/WebAssembly/wabt/releases/download/$VERSION/$ARCHIVE"
else
  echo "Neither wget nor curl found"
  exit 1
fi

mkdir -p "./wabt"
tar -xzf "$ARCHIVE" -C "./wabt" --strip-components=1

rm -f "./$ARCHIVE"

if [ -n "${GITHUB_PATH:-}" ]; then
  if command -v cygpath >/dev/null 2>&1; then
    TARGET_PATH="$(cygpath -w "$PWD/wabt/bin")"
  else
    TARGET_PATH="$PWD/wabt/bin"
  fi
  echo "$TARGET_PATH" >> "$GITHUB_PATH"
fi

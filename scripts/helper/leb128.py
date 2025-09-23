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

from typing import Tuple


MAX_LENGTH = 10


class u:
    @staticmethod
    def decode(b: bytes) -> Tuple[int, int]:
        """Decode the unsigned leb128 encoded bytearray"""
        r = 0
        i = None
        for i, e in enumerate(b):
            r = r + ((e & 0x7F) << (i * 7))
            if e & 0x80 == 0x00:
                break
        assert i is not None
        return r, i + 1


class i:
    @staticmethod
    def decode(b: bytes) -> Tuple[int, int]:
        """Decode the signed leb128 encoded bytearray"""
        r = 0
        i = None
        e = None
        for i, e in enumerate(b):
            r = r + ((e & 0x7F) << (i * 7))
            if e & 0x80 == 0x00:
                break
        assert e is not None and i is not None
        if e & 0x40 != 0:
            r |= -(1 << (i * 7) + 7)
        return r, i + 1

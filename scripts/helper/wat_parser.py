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

from typing import List, Tuple


def offset_to_position(lines: List[str], offset: int) -> Tuple[int, int]:
    cursor = 0
    for line_number in range(len(lines)):
        old_cursor = cursor
        cursor += len(lines[line_number]) + 1  # patch '\n' again
        if offset >= old_cursor and offset < cursor:
            return line_number, offset - old_cursor
    return len(lines), 0


class Range:
    """
    [start, end]
    """

    def __init__(self, start: int, end: int):
        self.start = start
        self.end = end

    def __repr__(self):
        return f"Range({self.start}, {self.end})"

    def to_string(self, wat) -> str:
        return wat[self.start : self.end + 1]

    def valid(self) -> bool:
        return self.start != -1 and self.end != -1


def split_sub_expr(wat: str, r: Range) -> List[Range]:
    """
    [begin, end]
    """
    subs = []
    current_start = None
    depth = -1
    for i in range(r.start, r.end + 1):
        if wat[i] == "(":
            depth += 1
            if depth == 1:
                current_start = i
        elif wat[i] == ")":
            depth -= 1
            if depth == 0:
                assert current_start is not None
                subs.append(Range(current_start, i))
                current_start = None
                continue
    return subs


def get_sub_expr(wat: str, r: Range) -> str:
    return wat[r.start + 1 : r.end].strip()


def extract_func_impl(wat: str, extract_range: Range) -> List[Range]:
    """
    Extracts function definitions from a wat string.

    Args:
        wat (str): The wat string to extract from.

    Returns:
        list: A list of function definitions.
    """
    assert len(wat) != 0
    ranges: List[Range] = []
    sub_ranges = split_sub_expr(wat, extract_range)
    for sub_range in sub_ranges:
        sub_expr = get_sub_expr(wat, sub_range)
        if sub_expr.startswith("func"):
            ranges.append(sub_range)
        # FIXME: Does here need recursive search?
    return ranges


def extract_func(wat: str) -> List[Range]:
    return extract_func_impl(wat, extract_range=Range(0, len(wat) - 1))


def has_memory(wat: str) -> bool:
    sub_ranges = split_sub_expr(wat, Range(0, len(wat) - 1))
    for sub_range in sub_ranges:
        sub_expr = get_sub_expr(wat, sub_range)
        if sub_expr.startswith("memory"):
            return True
    return False

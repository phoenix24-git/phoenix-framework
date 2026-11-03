# Helpers extracted verbatim from Godot Engine's methods.py (lines "generate_copyright_header"
# through "to_raw_cstring"), so Godot's own code generators can run without SCons.
#
# Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md).
# Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.
# SPDX-License-Identifier: MIT  (full text: engine/thirdparty/phoenix_core/LICENSE.txt)
from __future__ import annotations

import contextlib
import re
import os
import sys
import zlib
from io import StringIO
from typing import Generator, TextIO


def print_error(*values: object) -> None:
    print("ERROR:", *values, file=sys.stderr)


def print_warning(*values: object) -> None:
    print("WARNING:", *values, file=sys.stderr)


def print_info(*values: object) -> None:
    print(*values)


def generate_copyright_header(filename: str) -> str:
    MARGIN = 70
    TEMPLATE = """\
/**************************************************************************/
/*  %s*/
/**************************************************************************/
/*                         This file is part of:                          */
/*                             GODOT ENGINE                               */
/*                        https://godotengine.org                         */
/**************************************************************************/
/* Copyright (c) 2014-present Godot Engine contributors (see AUTHORS.md). */
/* Copyright (c) 2007-2014 Juan Linietsky, Ariel Manzur.                  */
/*                                                                        */
/* Permission is hereby granted, free of charge, to any person obtaining  */
/* a copy of this software and associated documentation files (the        */
/* "Software"), to deal in the Software without restriction, including    */
/* without limitation the rights to use, copy, modify, merge, publish,    */
/* distribute, sublicense, and/or sell copies of the Software, and to     */
/* permit persons to whom the Software is furnished to do so, subject to  */
/* the following conditions:                                              */
/*                                                                        */
/* The above copyright notice and this permission notice shall be         */
/* included in all copies or substantial portions of the Software.        */
/*                                                                        */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,        */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF     */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. */
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY   */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,   */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE      */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                 */
/**************************************************************************/
"""
    if len(filename := os.path.basename(filename).ljust(MARGIN)) > MARGIN:
        print_warning(f'Filename "{filename}" too large for copyright header.')
    return TEMPLATE % filename


@contextlib.contextmanager
def generated_wrapper(
    path: str,
    guard: bool | None = None,
) -> Generator[TextIO, None, None]:
    """
    Wrapper class to automatically handle copyright headers and header guards
    for generated scripts. Meant to be invoked via `with` statement similar to
    creating a file.

    - `path`: The path of the file to be created.
    - `guard`: Optional bool to determine if `#pragma once` should be added. If
    unassigned, the value is determined by file extension.
    """

    with open(path, "wt", encoding="utf-8", newline="\n") as file:
        if not path.endswith(".out"):  # For test output, we only care about the content.
            file.write(generate_copyright_header(path))
            file.write("\n/* THIS FILE IS GENERATED. EDITS WILL BE LOST. */\n\n")

            if guard is None:
                guard = path.endswith((".h", ".hh", ".hpp", ".hxx", ".inc"))
            if guard:
                file.write("#pragma once\n\n")

        with StringIO(newline="\n") as str_io:
            yield str_io
            file.write(str_io.getvalue().strip() or "/* NO CONTENT */")

        file.write("\n")


def get_buffer(path: str) -> bytes:
    with open(path, "rb") as file:
        return file.read()


def compress_buffer(buffer: bytes) -> bytes:
    # Use maximum zlib compression level to further reduce file size
    # (at the cost of initial build times).
    return zlib.compress(buffer, zlib.Z_BEST_COMPRESSION)


def format_buffer(buffer: bytes, indent: int = 0, width: int = 120) -> str:
    return re.sub(f"(.{{0,{width - indent - 1}}},) ", ("\t" * indent) + "\\g<1>\n", ", ".join(map(str, buffer)))


############################################################
# CSTRING PARSING
############################################################

C_ESCAPABLES = [
    ("\\", "\\\\"),
    ("\a", "\\a"),
    ("\b", "\\b"),
    ("\f", "\\f"),
    ("\n", "\\n"),
    ("\r", "\\r"),
    ("\t", "\\t"),
    ("\v", "\\v"),
    # ("'", "\\'"),  # Skip, as we're only dealing with full strings.
    ('"', '\\"'),
]
C_ESCAPE_TABLE = str.maketrans(dict((x, y) for x, y in C_ESCAPABLES))


def to_escaped_cstring(value: str) -> str:
    return value.translate(C_ESCAPE_TABLE)


def to_raw_cstring(value: str | list[str]) -> str:
    MAX_LITERAL = 16 * 1024

    if isinstance(value, list):
        value = "\n".join(value) + "\n"

    split: list[bytes] = []
    offset = 0
    encoded = value.encode()

    while offset <= len(encoded):
        segment = encoded[offset : offset + MAX_LITERAL]
        offset += MAX_LITERAL
        if len(segment) == MAX_LITERAL:
            # Try to segment raw strings at double newlines to keep readable.
            pretty_break = segment.rfind(b"\n\n")
            if pretty_break != -1:
                segment = segment[: pretty_break + 1]
                offset -= MAX_LITERAL - pretty_break - 1
            # If none found, ensure we end with valid utf8.
            # https://github.com/halloleo/unicut/blob/master/truncate.py
            elif segment[-1] & 0b10000000:
                last_11xxxxxx_index = [i for i in range(-1, -5, -1) if segment[i] & 0b11000000 == 0b11000000][0]
                last_11xxxxxx = segment[last_11xxxxxx_index]
                if not last_11xxxxxx & 0b00100000:
                    last_char_length = 2
                elif not last_11xxxxxx & 0b0010000:
                    last_char_length = 3
                elif not last_11xxxxxx & 0b0001000:
                    last_char_length = 4

                if last_char_length > -last_11xxxxxx_index:
                    segment = segment[:last_11xxxxxx_index]
                    offset += last_11xxxxxx_index

        split += [segment]

    if len(split) == 1:
        return f'R"<!>({split[0].decode()})<!>"'
    else:
        # Wrap multiple segments in parenthesis to suppress `string-concatenation` warnings on clang.
        return "({})".format(" ".join(f'R"<!>({segment.decode()})<!>"' for segment in split))



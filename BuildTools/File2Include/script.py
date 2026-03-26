# ----------------------------------------------------------------------------
# Copyright 2019-2026 Diligent Graphics LLC
#
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
#
# In no event and under no legal theory, whether in tort (including negligence),
# contract, or otherwise, unless required by applicable law (such as deliberate
# and grossly negligent acts) or agreed to in writing, shall any Contributor be
# liable for any damages, including any direct, indirect, special, incidental,
# or consequential damages of any character arising as a result of this License or
# out of the use or inability to use the software (including but not limited to damages
# for loss of goodwill, work stoppage, computer failure or malfunction, or any and
# all other commercial damages or losses), even if such Contributor has been advised
# of the possibility of such damages.
# ----------------------------------------------------------------------------

import sys
import io
import argparse

def strip_c_comments(text):
    result = []
    current_line = []

    in_line_comment = False
    in_block_comment = False
    in_string = False
    in_char = False
    escape = False

    line_has_comment = False
    line_has_noncomment_code = False

    block_comment_buf = []
    line_comment_buf = []

    def append_char(c):
        nonlocal line_has_noncomment_code
        current_line.append(c)
        if not c.isspace():
            line_has_noncomment_code = True

    def append_text(s):
        for ch in s:
            append_char(ch)

    def should_preserve_block_comment(comment_text):
        inner = comment_text[2:-2].strip()
        return inner.startswith("format")

    def should_preserve_line_comment(comment_text):
        inner = comment_text[2:].strip()
        return inner.startswith("format")

    def flush_line(add_newline):
        nonlocal current_line, line_has_comment, line_has_noncomment_code

        if not (line_has_comment and not line_has_noncomment_code and "".join(current_line).strip() == ""):
            result.extend(current_line)
            if add_newline:
                result.append("\n")

        current_line = []
        line_has_noncomment_code = False
        line_has_comment = in_block_comment

    i = 0
    n = len(text)

    while i < n:
        c = text[i]
        nxt = text[i + 1] if i + 1 < n else ""

        if in_line_comment:
            if c == "\n":
                comment_text = "".join(line_comment_buf)
                if should_preserve_line_comment(comment_text):
                    append_text(comment_text)

                line_comment_buf = []
                in_line_comment = False
                flush_line(True)
                i += 1
                continue

            line_comment_buf.append(c)
            i += 1
            continue

        if in_block_comment:
            block_comment_buf.append(c)

            if c == "*" and nxt == "/":
                block_comment_buf.append("/")
                comment_text = "".join(block_comment_buf)

                in_block_comment = False
                block_comment_buf = []

                if should_preserve_block_comment(comment_text):
                    append_text(comment_text)

                i += 2
                continue

            i += 1
            continue

        if in_string:
            append_char(c)
            if escape:
                escape = False
            elif c == "\\":
                escape = True
            elif c == '"':
                in_string = False
            i += 1
            continue

        if in_char:
            append_char(c)
            if escape:
                escape = False
            elif c == "\\":
                escape = True
            elif c == "'":
                in_char = False
            i += 1
            continue

        if c == "/" and nxt == "/":
            line_has_comment = True
            in_line_comment = True
            line_comment_buf = ["//"]
            i += 2
            continue

        if c == "/" and nxt == "*":
            line_has_comment = True
            in_block_comment = True
            block_comment_buf = ["/*"]
            i += 2
            continue

        if c == '"':
            append_char(c)
            in_string = True
            i += 1
            continue

        if c == "'":
            append_char(c)
            in_char = True
            i += 1
            continue

        if c == "\n":
            flush_line(True)
            i += 1
            continue

        append_char(c)
        i += 1

    if in_line_comment:
        comment_text = "".join(line_comment_buf)
        if should_preserve_line_comment(comment_text):
            append_text(comment_text)

    if current_line:
        if not (line_has_comment and not line_has_noncomment_code and "".join(current_line).strip() == ""):
            result.extend(current_line)

    return "".join(result)


def prepare_content(text, strip_comments=False):
    if strip_comments:
        return strip_c_comments(text)
    return text


def convert_to_string_lines(text):
    special_chars = "'\"\\"
    output = []

    for line in text.splitlines():
        quoted = ['"']

        for c in line.rstrip():
            if c in special_chars:
                quoted.append("\\")
            quoted.append(c)

        quoted.append('\\n"\n')
        output.append("".join(quoted))

    return "".join(output)


def main():
    parser = argparse.ArgumentParser(
        description="Convert a file into quoted string lines."
    )
    parser.add_argument("src", help="source file")
    parser.add_argument("dst", help="destination file")
    parser.add_argument(
        "--strip-comments",
        action="store_true",
        help="strip // and /* */ comments before converting",
    )

    args = parser.parse_args()

    try:
        if args.src == args.dst:
            raise ValueError("Source and destination files must be different")

        with open(args.src, "r") as src_file, open(args.dst, "w") as dst_file:
            content = src_file.read()
            content = prepare_content(content, args.strip_comments)
            dst_file.write(convert_to_string_lines(content))

        print("File2String: successfully converted {} to {}".format(args.src, args.dst))

    except (ValueError, IOError) as error:
        print(error)
        sys.exit(1)


if __name__ == "__main__":
    main()

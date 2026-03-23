import unittest

from script import strip_c_comments, convert_to_string_lines


class StripCCommentsTests(unittest.TestCase):
    def test_keeps_plain_code(self):
        src = "int x = 1;\nfloat y = 2;\n"
        self.assertEqual(strip_c_comments(src), src)

    def test_removes_line_comment(self):
        src = "int x = 1; // comment\n"
        self.assertEqual(strip_c_comments(src), "int x = 1; \n")

    def test_removes_block_comment(self):
        src = "int /* comment */ x = 1;\n"
        self.assertEqual(strip_c_comments(src), "int  x = 1;\n")

    def test_removes_multiline_block_comment(self):
        src = "int x = 1; /* comment\nstill comment */\nfloat y = 2;\n"
        self.assertEqual(strip_c_comments(src), "int x = 1; \nfloat y = 2;\n")

    def test_removes_comment_only_line(self):
        src = "int x = 1;\n// comment only\nfloat y = 2;\n"
        self.assertEqual(strip_c_comments(src), "int x = 1;\nfloat y = 2;\n")

    def test_removes_whitespace_and_comment_only_line(self):
        src = "int x = 1;\n   // comment only\nfloat y = 2;\n"
        self.assertEqual(strip_c_comments(src), "int x = 1;\nfloat y = 2;\n")

    def test_preserves_comment_like_text_in_string(self):
        src = 'const char* s = "not // comment and not /* comment */";\n'
        self.assertEqual(strip_c_comments(src), src)

    def test_preserves_comment_like_text_in_char_literal(self):
        src = "char c = '/'; // real comment\n"
        self.assertEqual(strip_c_comments(src), "char c = '/'; \n")

    def test_preserves_escaped_quotes_in_string(self):
        src = 'const char* s = "\\"quoted\\" // still string"; // comment\n'
        expected = 'const char* s = "\\"quoted\\" // still string"; \n'
        self.assertEqual(strip_c_comments(src), expected)

    def test_preserves_format_block_comment(self):
        src = "RWTexture2D<float /* format = r32f */ >\n"
        self.assertEqual(strip_c_comments(src), src)

    def test_preserves_format_line_comment(self):
        src = "RWTexture2D<float // format = r32f\n>\n"
        self.assertEqual(strip_c_comments(src), src)

    def test_mixed_preserved_and_normal_comments(self):
        src = (
            "RWTexture2D<float /* format = r32f */ > // remove this\n"
            "int x = 1; /* remove this */\n"
        )
        expected = (
            "RWTexture2D<float /* format = r32f */ > \n"
            "int x = 1; \n"
        )
        self.assertEqual(strip_c_comments(src), expected)

    def test_escaped_backslash_at_end_of_string(self):
        # A backslash escaping another backslash shouldn't escape the quote
        src = 'const char* s = "backslash\\\\"; // comment\n'
        expected = 'const char* s = "backslash\\\\"; \n'
        self.assertEqual(strip_c_comments(src), expected)

    def test_comment_start_inside_char_literal(self):
        # Testing if it handles /* inside a character literal correctly
        src = "char a = '/'; char b = '*';\n"
        self.assertEqual(strip_c_comments(src), src)

    def test_block_comment_preserved_with_internal_stars(self):
        # Ensure the 'format' check doesn't fail if there are internal decorators
        src = "/* format: r32f ******* */\n"
        self.assertEqual(strip_c_comments(src), src)
        
    def test_multiple_block_comments_one_line(self):
        src = "/* remove */ code(); /* format: preserve */\n"
        expected = " code(); /* format: preserve */\n"
        self.assertEqual(strip_c_comments(src), expected)

    def test_multiline_block_comment_inline_does_not_insert_newline(self):
        src = "x /* comment\nstill comment */ y\n"
        self.assertEqual(strip_c_comments(src), "x  y\n")

    def test_multiline_preserved_block_comment_stays_inline(self):
        src = "x /* format = r32f\nstill format */ y\n"
        self.assertEqual(
            strip_c_comments(src),
            "x /* format = r32f\nstill format */ y\n",
        )

    def test_removes_line_comment_at_eof(self):
        src = "int x = 1; // comment"
        self.assertEqual(strip_c_comments(src), "int x = 1; ")

    def test_preserves_format_line_comment_at_eof(self):
        src = "x // format = r32f"
        self.assertEqual(strip_c_comments(src), "x // format = r32f")

class ConvertToStringLinesTests(unittest.TestCase):
    def test_converts_single_line(self):
        src = "hello\n"
        self.assertEqual(convert_to_string_lines(src), '"hello\\n"\n')

    def test_converts_multiple_lines(self):
        src = "hello\nworld\n"
        self.assertEqual(convert_to_string_lines(src), '"hello\\n"\n"world\\n"\n')

    def test_escapes_double_quotes(self):
        src = '"abc"\n'
        self.assertEqual(convert_to_string_lines(src), '"\\"abc\\"\\n"\n')

    def test_escapes_single_quotes(self):
        src = "'a'\n"
        self.assertEqual(convert_to_string_lines(src), '"\\\'a\\\'\\n"\n')

    def test_escapes_backslashes(self):
        src = "C:\\temp\\file\n"
        self.assertEqual(convert_to_string_lines(src), '"C:\\\\temp\\\\file\\n"\n')

    def test_preserves_empty_line(self):
        src = "\n"
        self.assertEqual(convert_to_string_lines(src), '"\\n"\n')

    def test_handles_text_without_trailing_newline(self):
        src = "hello"
        self.assertEqual(convert_to_string_lines(src), '"hello\\n"\n')

    def test_handles_text_with_trailing_spaces(self):
        src = "hello   "
        self.assertEqual(convert_to_string_lines(src), '"hello\\n"\n')

    def test_strip_then_convert_integration(self):
        src = 'int x = 1; // comment\nconst char* s = "a";\n'
        stripped = strip_c_comments(src)
        expected = (
            '"int x = 1;\\n"\n'
            '"const char* s = \\"a\\";\\n"\n'
        )
        self.assertEqual(convert_to_string_lines(stripped), expected)

    def test_empty_input(self):
        self.assertEqual(convert_to_string_lines(""), "")

    def test_preserves_blank_line_in_middle(self):
        src = "a\n\nb\n"
        self.assertEqual(convert_to_string_lines(src), '"a\\n"\n"\\n"\n"b\\n"\n')

    def test_trims_trailing_tabs_too(self):
        src = "hello\t\t\n"
        self.assertEqual(convert_to_string_lines(src), '"hello\\n"\n')

if __name__ == "__main__":
    unittest.main()

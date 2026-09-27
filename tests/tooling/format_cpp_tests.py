"""Regression checks for whitespace-only conditional formatting."""

import importlib.util
import pathlib
import unittest


spec = importlib.util.spec_from_file_location(
    'format_cpp', pathlib.Path(__file__).resolve().parents[2] / 'tools/format_cpp.py'
)
formatter = importlib.util.module_from_spec(spec)
spec.loader.exec_module(formatter)


class ConditionalFormattingTests(unittest.TestCase):
    def check_format(self, source, expected):
        result = formatter.format_ternaries(source)
        self.assertEqual(result, expected)
        self.assertEqual(formatter.format_ternaries(result), result)
        self.assertEqual(
            [t.group() for t in formatter.TOKENS.finditer(source)],
            [t.group() for t in formatter.TOKENS.finditer(result)],
        )

    def test_short(self):
        source = '  return a ? b : c;\n'
        self.check_format(source, source)

    def test_long_argument(self):
        self.check_format(
            '      condition ? "first value"\n                : "second value"\n',
            '      condition\n        ? "first value"\n        : "second value"\n',
        )

    def test_existing_break(self):
        source = '  value = condition\n    ? first\n    : second;\n'
        self.check_format(source, source)

    def test_nested(self):
        self.check_format(
            '  return a ? b : c ? d\n                      : e;\n',
            '  return a\n    ? b\n    : c\n      ? d\n      : e;\n',
        )

    def test_scope_and_lambda(self):
        source = '  auto x = a ? [] { label: return T::value; }() : T::fallback;\n'
        self.check_format(source, source)

    def test_opaque_tokens(self):
        source = '#define X a ? b : c\n// ? :\n/* ? : */\nauto s = R"tag(? : ")tag";\n'
        self.check_format(source, source)

    def test_separate_arguments(self):
        source = '  f(a ? b : c,\n    d ? e : f);\n'
        self.check_format(source, source)

    def test_long_constructor(self):
        self.check_format(
            'namespace N {\nType::Type()\n  : delay_(a ? b\n             : c) {}\n}\n',
            'namespace N {\nType::Type()\n  : delay_(a\n    ? b\n    : c) {}\n}\n',
        )


if __name__ == '__main__':
    unittest.main()

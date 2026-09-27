"""Run clang-format, then format multiline C++ conditional operators."""

import argparse
import pathlib
import re
import subprocess
import sys


TOKENS = re.compile(
    r'(?P<opaque>^[ \t]*\#(?:[^\n]*\\\n)*[^\n]*'
    r'|//[^\n]*|/\*.*?\*/'
    r'|(?:u8|u|U|L)?R"(?P<delimiter>[^ ()\\\t\r\n]{0,16})'
    r'\(.*?\)(?P=delimiter)"'
    r'|(?:u8|u|U|L)?"(?:\\.|[^"\\])*"'
    r"|(?:u8|u|U|L)?'(?:\\.|[^'\\])*')"
    r"|[0-9][A-Za-z0-9_'.]*|[A-Za-z_][A-Za-z_0-9]*|::|[^\s]",
    re.MULTILINE | re.DOTALL,
)


def format_ternaries(source):
    """Re-indent only conditional expressions that clang-format made multiline."""
    tokens = []
    depth = []
    questions = []
    pairs = []
    for token in TOKENS.finditer(source):
        value = token.group()
        before = tuple(depth)
        tokens.append((token, before))
        if token.group('opaque') is not None:
            continue
        if value == '?':
            questions.append((len(tokens) - 1, before))
        elif value == ':' and questions and questions[-1][1] == before:
            question, _ = questions.pop()
            pairs.append((question, len(tokens) - 1))
        if value in ('(', '[', '{'):
            depth.append(value)
        elif value in (')', ']', '}') and depth:
            depth.pop()

    replacements = []
    formatted_pairs = []
    for question, colon in sorted(pairs):
        q, level = tokens[question]
        c, _ = tokens[colon]
        end = len(source)
        for token, token_level in tokens[colon + 1:]:
            if len(token_level) < len(level) or (
                token_level == level and token.group() in (',', ';', ')', ']', '}')
            ):
                end = token.start()
                break
        previous = tokens[question - 1][0] if question else q
        # Exclude whitespace after the expression, e.g. before the next argument.
        multiline = '\n' in source[q.start():end].rstrip()
        multiline = multiline or '\n' in source[previous.end():q.start()]
        if not multiline:
            continue
        # A pre-existing operator-only line inherits the condition's indentation.
        line_start = source.rfind('\n', 0, q.start()) + 1
        if not source[line_start:q.start()].strip():
            line_start = source.rfind('\n', 0, previous.start()) + 1
        indent = re.match(r'[ \t]*', source[line_start:]).group() + '  '
        for parent_end, parent_indent in reversed(formatted_pairs):
            if q.start() < parent_end:
                indent = parent_indent + '  '
                break
        formatted_pairs.append((end, indent))
        for operator in (q, c):
            start = operator.start()
            while start > 0 and source[start - 1].isspace():
                start -= 1
            replacements.append((start, operator.start(), '\n' + indent))
    for start, end, replacement in sorted(replacements, reverse=True):
        source = source[:start] + replacement + source[end:]
    return source


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--clang-format', required=True)
    parser.add_argument('--check', action='store_true')
    parser.add_argument('files', nargs='+', type=pathlib.Path)
    options = parser.parse_args()
    different = False
    for path in options.files:
        # Normalize line endings for comparison; preserve the file's existing style on write.
        original_bytes = path.read_bytes()
        original = original_bytes.decode('utf-8').replace('\r\n', '\n')
        formatted = subprocess.run(
            [options.clang_format, '--style=file', '--fallback-style=none', str(path)],
            check=True, capture_output=True,
        ).stdout.decode('utf-8').replace('\r\n', '\n')
        formatted = format_ternaries(formatted)
        if formatted == original:
            continue
        different = True
        if options.check:
            print(f'{path}: formatting differs', file=sys.stderr)
        else:
            newline = '\r\n' if b'\r\n' in original_bytes else '\n'
            path.write_bytes(formatted.replace('\n', newline).encode('utf-8'))
    return int(different and options.check)


if __name__ == '__main__':
    sys.exit(main())

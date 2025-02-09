import argparse
from dataclasses import dataclass
from enum import Enum


class TokenKind(Enum):
    UNDEFINED = 0
    KEY = 1
    DELIM = 2
    VALUE = 3


@dataclass
class Token:
    kind: TokenKind
    value: str
    line: int


def str_find_any(s, chars):
    for c in chars:
        i = s.find(c)
        if i != -1:
            return i, c
    return -1, ""


kSpaces = " \t"

kKeyValSep = "="
kKeyValEnd = ";"
kCommentBegin = "#"
kStringBegin = '"'
kRawStringBegin = "`"
kListBegin = "["
kListEnd = "]"
kTableBegin = "{"
kTableEnd = "}"


class Scanner:
    def __init__(self, filepath: str, content: str):
        self.filepath = filepath
        self.content = content
        self.tokens = []
        self.line = 1
        self.error = ""

    def scan(self):
        while len(self.content) != 0:
            self.trim_space()
            ok = False
            if self.expect("\n"):
                ok = self.scan_newline()
            elif self.expect(kCommentBegin):
                ok = self.scan_comment()
            else:
                ok = self.scan_key_value()
            if not ok:
                return None
        return self.tokens

    def scan_newline(self) -> bool:
        self.line += 1
        self.advance(1)
        return True

    def scan_comment(self) -> bool:
        end = self.content.find("\n")
        if end == -1:
            self.errorf("comment does not end with newline")
            return False
        self.advance(end)
        return True

    def scan_key_value(self) -> bool:
        if not self.scan_key():
            return False
        self.append_delim()
        if not self.scan_value():
            return False
        return True

    def scan_key(self) -> bool:
        end, c = str_find_any(self.content, "=\n")
        if end == -1 or c != kKeyValSep:
            self.errorf("key-value pair is missing separator")
            return False
        self.append(TokenKind.KEY, end)
        if len(self.tokens[-1].value) == 0:
            self.errorf("empty key")
            return False
        return True

    def scan_value(self) -> bool:
        self.trim_space()
        ok = False
        if self.expect(kListBegin):
            ok = self.scan_list_value()
        elif self.expect(kTableBegin):
            ok = self.scan_table_value()
        elif self.expect(kStringBegin):
            ok = self.scan_string_value()
        elif self.expect(self, kRawStringBegin):
            ok = self.scan_raw_string()
        else:
            ok = self.scan_int_or_bool_value()
        if not ok:
            return False
        if not self.scan_delim(kKeyValEnd):
            self.errorf("value does not end with semicolon")
            return False
        return True

    def scan_delim(self, c: str) -> bool:
        if not self.expect(c):
            return False
        self.append_delim()
        return True

    def errorf(self, s: str):
        self.error = f"{self.filepath}:{self.line}: error: scan: {s}"

    def append(self, kind: TokenKind, end: int):
        val = self.content[:end].rstrip(kSpaces)
        self.tokens.append(
            Token(
                kind=kind,
                value=val,
                line=self.line,
            )
        )
        self.advance(end)

    def append_delim(self):
        self.tokens.append(
            Token(
                kind=TokenKind.DELIM,
                value=self.content[:1],
                line=self.line,
            )
        )
        self.advance(1)

    def trim_space(self):
        self.content = self.content.lstrip(kSpaces)

    def expect(self, c) -> bool:
        if len(self.content) == 0:
            return False
        return self.content[0] == c

    def advance(self, n: int):
        self.content = self.content[n:]


parser = argparse.ArgumentParser(
    prog="ProgramName",
    description="What the program does",
    epilog="Text at the bottom of help",
)

parser.add_argument("filepath")

args = parser.parse_args()

content = ""
with open(args.filepath, "r") as f:
    content = f.read()

s = Scanner(args.filepath, content)
tokens = s.scan()
if tokens is None:
    print(s.error)
else:
    for token in tokens:
        print(token.kind, token.value, token.line)

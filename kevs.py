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
kNewline = "\n"
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
            if self.expect(kNewline):
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
        end = self.content.find(kNewline)
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
        elif self.expect(kRawStringBegin):
            ok = self.scan_raw_string()
        else:
            ok = self.scan_int_or_bool_value()
        if not ok:
            return False
        if not self.scan_delim(kKeyValEnd):
            self.errorf("value does not end with semicolon")
            return False
        return True

    def scan_list_value(self) -> bool:
        self.append_delim()
        while True:
            self.trim_space()
            if len(self.content) == 0:
                self.errorf("end of input without list end")
                return False
            if self.expect(kNewline):
                if not self.scan_newline():
                    return False
                continue
            if self.expect(kCommentBegin):
                if not self.scan_comment():
                    return False
                continue
            if self.expect(kListEnd):
                self.append_delim()
                return True
            if not self.scan_value():
                return False
            if self.expect(kListEnd):
                self.append_delim()
                return True
        return True

    def scan_raw_string(self) -> bool:
        end = self.content[1:].find(kRawStringBegin)
        if end == -1:
            self.errorf("raw string value does not end with backtick")
            return False

        # +2 for leading and trailing quotes
        self.append(TokenKind.VALUE, end + 2)

        # count newlines in raw string to keep line count accurate
        self.line += self.tokens[-1].value.count("\n")

        return True

    def scan_table_value(self) -> bool:
        self.append_delim()
        while True:
            self.trim_space()
            if len(self.content) == 0:
                self.errorf("end of input without table end")
                return False
            if self.expect(kNewline):
                if not self.scan_newline():
                    return False
                continue
            if self.expect(kCommentBegin):
                if not self.scan_comment():
                    return False
                continue
            if self.expect(kTableEnd):
                self.append_delim()
                return True
            if not self.scan_key_value():
                return False
            if self.expect(kTableEnd):
                self.append_delim()
                return True
        return True

    def scan_string_value(self) -> bool:
        # advance past leading quote
        s = self.content[1:]

        while True:
            # search for trailing quote
            i = s.find(kStringBegin)
            if i == -1:
                self.errorf("string value does not end with quote")
                return False

            # get previous char
            prev = s[i - 1]

            # advance
            s = s[i + 1 :]

            # stop if quote is not escaped
            if prev != "\\":
                break

        # calculate the end, includes trailing quote
        end = self.content.find(s) - 1

        # +1 for leading quote
        self.append(TokenKind.VALUE, end + 1)

        return True

    def scan_int_or_bool_value(self) -> bool:
        # search for all possible value endings
        # if semicolon(or none of them) is not found => error
        end, c = str_find_any(self.content, ";]}\n")
        if end == -1 or c != kKeyValEnd:
            self.errorf("integer or boolean value does not end with semicolon")
            return False
        self.append(TokenKind.VALUE, end)
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


parser = argparse.ArgumentParser(prog="kevs")
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
        print(f"{token.kind}\t{token.value}")

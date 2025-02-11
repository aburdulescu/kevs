from dataclasses import dataclass
from enum import Enum
from typing import Any


class TokenKind(Enum):
    undefined = 0
    key = 1
    delim = 2
    value = 3


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
    return -1, None


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
    def __init__(self, file: str, content: str):
        self.file = file
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
        self.append(TokenKind.key, end)
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
        self.append(TokenKind.value, end + 2)

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
        self.append(TokenKind.value, end + 1)

        return True

    def scan_int_or_bool_value(self) -> bool:
        # search for all possible value endings
        # if semicolon(or none of them) is not found => error
        end, c = str_find_any(self.content, ";]}\n")
        if end == -1 or c != kKeyValEnd:
            self.errorf("integer or boolean value does not end with semicolon")
            return False
        self.append(TokenKind.value, end)
        return True

    def scan_delim(self, c: str) -> bool:
        if not self.expect(c):
            return False
        self.append_delim()
        return True

    def errorf(self, s: str):
        self.error = f"{self.file}:{self.line}: error: scan: {s}"

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
                kind=TokenKind.delim,
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


class ValueKind(Enum):
    undefined = 0
    string = 1
    integer = 2
    boolean = 3
    list = 4
    table = 5


@dataclass
class Value:
    kind: ValueKind
    data: Any


@dataclass
class KeyValue:
    key: str
    val: Value


def is_digit(c: str) -> bool:
    assert len(c) == 1
    return c in "0123456789"


def is_letter(c: str) -> bool:
    assert len(c) == 1
    c = c.lower()
    return c in "abcdefghijklmnopqrstuvwxyz"


def is_identifier(key: str) -> bool:
    c = key[0]
    if c != "_" and not is_letter(c):
        return False
    for c in key:
        if not is_digit(c) and not is_letter(c) and c != "_":
            return False
    return True


class Parser:
    def __init__(self, file: str, content: str, tokens: []):
        self.file = file
        self.content = content
        self.tokens = tokens
        self.i = 0
        self.error = ""

    def parse(self):
        table = []
        while self.i < len(self.tokens):
            kv, ok = self.parse_key_value(table)
            if not ok:
                return None
            table.append(kv)
        return table

    def parse_key_value(self, parent) -> (KeyValue, bool):
        key, ok = self.parse_key(parent)
        if not ok:
            return None, False

        if not self.parse_delim(kKeyValSep):
            self.errorf("missing key value separator")
            return None, False

        val, ok = self.parse_value()
        if not ok:
            return None, False

        return KeyValue(key=key, val=val), True

    def parse_key(self, parent) -> (str, bool):
        if not self.expect(TokenKind.key):
            self.errorf("expected key token")
            return None, False

        tok = self.get()
        if not is_identifier(tok.value):
            self.errorf(f"key is not a valid identifier: '{tok.value}'")
            return None, False

        # check if key is unique
        for entry in parent:
            if entry.key == tok.value:
                self.errorf(f"key '{tok.value}' is not unique for current table")
                return None, False

        key = tok.value

        self.pop()

        return key, True

    def parse_value(self) -> (Value, bool):
        ok = False
        val = None
        if self.expect_delim(kListBegin):
            val, ok = self.parse_list_value()
        elif self.expect_delim(kTableBegin):
            val, ok = self.parse_table_value()
        else:
            val, ok = self.parse_simple_value()
        if not ok:
            return None, False
        if not self.parse_delim(kKeyValEnd):
            self.errorf("missing key value end")
            return None, False
        return val, True

    def parse_simple_value(self) -> (Value, bool):
        if not self.expect(TokenKind.value):
            self.errorf("expected value token")
            return None, False

        val = self.get().value

        ok = True
        out = None

        if val.startswith(kStringBegin):
            try:
                out = Value(
                    kind=ValueKind.string,
                    data=bytes(val[1:-1], "utf-8").decode("unicode_escape"),
                )
            except UnicodeDecodeError as e:
                self.errorf(f"could not normalize string: {str(e)}")
                ok = False
        elif val.startswith(kRawStringBegin):
            out = Value(
                kind=ValueKind.string,
                data=val[1:-1],
            )
        elif val == "true":
            out = Value(
                kind=ValueKind.boolean,
                data=True,
            )
        elif val == "false":
            out = Value(
                kind=ValueKind.boolean,
                data=False,
            )
        else:
            try:
                out = Value(
                    kind=ValueKind.integer,
                    data=int(val, 0),
                )
            except ValueError as e:
                self.errorf(f"value '{val}' is not an integer: {str(e)}")
                ok = False

        self.pop()

        return out, ok

    def parse_list_value(self) -> (Value, bool):
        out = Value(
            kind=ValueKind.list,
            data=[],
        )

        self.pop()

        while True:
            if self.parse_delim(kListEnd):
                return out, True

            v, ok = self.parse_value()
            if not ok:
                return None, False
            out.data.append(v)

            if self.parse_delim(kListEnd):
                return out, True

        return out, True

    def parse_table_value(self) -> (Value, bool):
        out = Value(
            kind=ValueKind.table,
            data=[],
        )

        self.pop()

        while True:
            if self.parse_delim(kTableEnd):
                return out, True

            v, ok = self.parse_key_value(out.data)
            if not ok:
                return None, False
            out.data.append(v)

            if self.parse_delim(kTableEnd):
                return out, True

        return out, True

    def parse_delim(self, c: str) -> bool:
        if not self.expect_delim(c):
            return False
        self.pop()
        return True

    def expect_delim(self, delim: str) -> bool:
        if not self.expect(TokenKind.delim):
            return False
        return self.get().value == delim

    def expect(self, kind: TokenKind) -> bool:
        if self.i >= len(self.tokens):
            self.errorf(f"expected token '{kind.name}', have nothing")
            return False
        return self.get().kind == kind

    def get(self) -> Token:
        return self.tokens[self.i]

    def pop(self):
        self.i += 1

    def errorf(self, s: str):
        self.error = f"{self.file}:{self.tokens[self.i].line}: error: parse: {s}"


def list_dump(root):
    for v in root:
        if v.kind == ValueKind.table:
            print(v.kind.name)
            table_dump(v.data)
        elif v.kind == ValueKind.list:
            print(v.kind.name)
            list_dump(v.data)
        elif v.kind == ValueKind.string:
            print(f"{v.kind.name} '{v.data}'")
        elif v.kind == ValueKind.boolean:
            print(v.kind.name, "true" if v.data else "false")
        else:
            print(v.kind.name, v.data)


def table_dump(root):
    for kv in root:
        if kv.val.kind == ValueKind.table:
            print(kv.key, kv.val.kind.name)
            table_dump(kv.val.data)
        elif kv.val.kind == ValueKind.list:
            print(kv.key, kv.val.kind.name)
            list_dump(kv.val.data)
        elif kv.val.kind == ValueKind.string:
            print(f"{kv.key} {kv.val.kind.name} '{kv.val.data}'")
        elif kv.val.kind == ValueKind.boolean:
            print(kv.key, kv.val.kind.name, "true" if kv.val.data else "false")
        else:
            print(kv.key, kv.val.kind.name, kv.val.data)

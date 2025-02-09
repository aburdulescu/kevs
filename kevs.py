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


class Scanner:
    spaces = " \t"

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
            if self.expect('\n'):
                ok = self.scan_newline()
            elif self.expect('#'):
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
        n = self.content.find('\n')
        if n == -1:
            self.error = "comment does not end with newline"
            return False
        self.advance(n)
        return True

    def scan_key_value(self) -> bool:
        if not self.scan_key():
            return False

        self.append_delim()

        if not self.scan_value():
            return False

        return True

    def scan_key(self) -> bool:
        # TODO
        return True

    def scan_value(self) -> bool:
        # TODO
        return True

    def append(self, kind: TokenKind, end: int):
        val = self.content[:end].rstrip(self.spaces)
        self.tokens.append(Token(
            kind=kind,
            value=val,
            line=self.line,
        ))
        self.advance(end)

    def append_delim(self):
        self.tokens.append(Token(
            kind=TokenKind.DELIM,
            value=self.content[:1],
            line=self.line,
        ))
        self.advance(1)

    def trim_space(self):
        self.content = self.content.lstrip(self.spaces)

    def expect(self, c) -> bool:
        if len(self.content) == 0:
            return False
        return self.content[0] == c

    def advance(self, n: int):
        self.content = self.content[n:]

parser = argparse.ArgumentParser(
                    prog='ProgramName',
                    description='What the program does',
                    epilog='Text at the bottom of help')

parser.add_argument('filepath')

args = parser.parse_args()

content = ""
with open(args.filepath, "r") as f:
    content = f.read()

s = Scanner(args.filepath, content)
tokens = s.scan()
if not tokens:
    print("error:", s.error)
else:
    for token in tokens:
        print(token.kind, token.value, token.line)

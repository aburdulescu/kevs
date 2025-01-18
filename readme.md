# KEVS

A configuration format which aims to be easy(enough) to read and write by humans and at the same time, easy(enough) to parse by machines.

## Features

- line comments: `# comment`
- key-value pairs: `key = value;`

### Keys

Keys are valid identifiers: `[_a-zA-Z][_a-zA-Z0-9]*`.

### Values

#### String

Strings must contain only valid UTF-8 characters.

There are two type of strings: raw and interpreted.

Interpreted strings are enclosed in `""`.
Escape sequences are interpreted and replaced in the final string.

Raw strings are enclosed in ` `` `.
The final string is as written, escape sequences are ignored.

```
string_escaped = "first line\nsecond\n\tthird has a tab\nSpock says: \U0001F596"

raw_string = `first line
second
    third has a tab
Spock says: 🖖`;
```

In the example from above, the two strings are equal: `string_escaped == raw_string`

The supported escape sequences are:

```
\b         - backspace       (U+0008)
\t         - tab             (U+0009)
\n         - linefeed        (U+000A)
\f         - form feed       (U+000C)
\r         - carriage return (U+000D)
\"         - quote           (U+0022)
\\         - backslash       (U+005C)
\uXXXX     - unicode         (U+XXXX)
\UXXXXXXXX - unicode         (U+XXXXXXXX)
```

#### Integer

```
x1 = 42;  # positive(implicit)
x2 = +42; # positive(explicit)
x3 = -42; # negative

x4 = 0x2a;     # hexadeciaml
x5 = 0o52;     # octal
x6 = 0b101010; # binary

# same but signed
x4 = -0x2a;     # hexadeciaml
x5 = +0o52;     # octal
x6 = -0b101010; # binary
```

#### Boolean:

```
x = true;
y = false;
```

#### List(of values)

```
x = [
  "foo";
  "bar";
  "baz";
];

y = [1; 2; 3; ];
```

#### Table(of key-value pairs):

```
x = {
  a = 23;
  b = "42";
};

y = {foo = true; bar = 0xcafe; };
```

## Examples

- Basic example: [example.kevs](./examples/example.kevs).
- [TOML example](./examples/toml.toml) from toml.io compared with its [KEVS equivalent](./examples/toml.kevs)

## Possible FAQ

### What's with the trailing `;`?

You can see that all values must end with a `;`.
This may be anoying for the user, but is intentionally enforced to make the parser code simpler.
This is a tradeoff between user's convenience and the simplicity of the implementation.

### Where are the float numbers?

There aren't any.
You can store them as integers then convert them back yourself: `f = 123.45 => i = f*100 => f = i/100`.
Or, store them as string and parse them in your application.

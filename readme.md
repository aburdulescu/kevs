# KEVS(`k=v;`)

A configuration format which aims to be easy(enough) to read and write by humans and, at the same time, easy(enough) to parse by machines.

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
\a   U+0007 alert or bell
\b   U+0008 backspace
\f   U+000C form feed
\n   U+000A line feed or newline
\r   U+000D carriage return
\t   U+0009 horizontal tab
\v   U+000B vertical tab
\\   U+005C backslash
\"   U+0022 double quote

\uXXXX       U+XXXX     unicode
\UXXXXXXXX   U+XXXXXXXX unicode
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

## Implementations

- [C](./src/c)
- [Python(partial)](./src/py)
- [Go](https://github.com/aburdulescu/gokevs)

The C implementation can be used by in any language which can interoperate with C(see examples for Go and Zig).

## Possible FAQ

### What's with the trailing `;`?

You can see that all values must end with a `;`.
This may be anoying for the user, but is intentionally enforced to make the parser code simpler.
This is a tradeoff between user's convenience and the simplicity of the implementation.

### Where are the float numbers?

There aren't any.
You can store them as integers then convert them back yourself: `f = 123.45 => i = f*100 => f = i/100`.
Or, store them as string and parse them in your application.

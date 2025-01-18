# KEVS

A configuration format which aims to be easy(enough) to read and write by humans and at the same time, easy(enough) to parse by machines.

## Features

- line comments: `# comment`
- key-value pairs: `key = value;`

### Keys

Keys are valid identifiers: `[_a-zA-Z][_a-zA-Z0-9]*`.

### Values

- string(interpreted):

```
x = "first line\nsecond\n\tthird has a tab\nSpock says: \U001F596"
```

- string(raw):

```
x = `first line
second
    third has a tab
Spock says: 🖖`;
```

- integer:

```
x1 = 42;
x2 = +42;
x3 = -42;
x4 = 0x2a; # hex
x5 = 0o52; # octal
x6 = 0b101010; # binary
```

- boolean:

```
x = true;
y = false;
```

- list(of values)

```
x = [
  "foo";
  "bar";
  "baz";
];

y = [1; 2; 3; ];
```

- table(of key-value pairs):

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

# KEVS/`k=v;`/Key Equals Value Semicolon

## DISCLAIMER!!!

This is not a usable project, it's just a toy implementation which works
on most valid input but is not very well tested.

So, DO NOT USE IT for anything serious!

## What?

A data/configuration format which aims to be easy(enough) to read and write by humans and at the same time, easy(enough) to parse by machines.

## Why?

Because:

- XML is too verbose
- JSON has no comments, no trailing commas
- YAML is too complex
- TOML is good enough, altough, for me personally, the syntax for tables is too clunky and it has too many features(I don't need date and time)
- INI/CONF is good, but no spec and not enough types

Whenever I need to store some configuration in a file, I always go through that list and choose one but without any joy.

This format I propose is the best, for me.

## Features

- line comments: `# comment`
- key-value pairs: `key = value;`

### Keys

Keys are valid identifiers: `[_a-zA-Z][_a-zA-Z0-9]*`.

Why? TOML maps to a hash table, KEVS maps to a struct.

### Values

- string:
```
x = "foo";
```

- multiline string:
```
x = `first line
second
and
so
on
`;
```

- integer:
```
x1 = 42;
x2 = +42;
x3 = -42;
x4 = 0x2a;
x5 = 0o52;
x6 = 0b101010;
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
  b = "42"";
};

y = {foo = true; bar = 0xcafe; };
```

## Example

See [example](./example.kevs).

## Possible FAQ

### What's with the trailing `;`? It's anoying!

You can see that all values must end with a `;`.
This is very likely very anoying for the user, but is intentionally enforced to make the parser code simpler.
This is a tradeoff between user's convenience and the simplicity of the implementation.
I, personally, accept it gladly. I would gladly type that extra `;` if it means I can be more confident that by typing it, the programmer who implemented the parser had one less(or more) corner cases to consider.

### This is not fully specified! What about escaped strings? Or ...?

This is a project tought of and implemented in a few days over the holidays, not a production grade format.

Maybe I'll keep working on it if there's interest.

Most likely not :).

### Where are the float numbers?

There aren't any. At the moment, I don't see their need. Don't want to complicate the code.

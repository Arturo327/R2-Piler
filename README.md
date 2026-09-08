# R2-Piler

A small compiler for the (invented) **R2-Lang** language, written in C.

## Current Status

Currently implements a complete lexer with arena-based memory management. The parser, type checker, and code generator are not built yet.

**Early stage / work in progress.**

- Lexer — implemented and tested
- Parser — not started
- Type checker — not started
- Code generation — not started

For now, `--dump-tokens` is the main way to see the compiler do something.

## Features

- Lexer: identifiers, keywords, integer/char/string literals (with escape sequences), `//` comments, and the full set of operators (arithmetic, bitwise, logical, comparison).
- Precise error reporting with `file:line:column` locations.
- Arena allocator — all compiler memory is freed in a single call at program exit instead of scattered `malloc`/`free` calls.
- `--dump-tokens` flag to inspect exactly what the lexer produces for a given source file.
- Fixture-based test runner (`make test`) that checks stdout, stderr, and exit status.

## Quick Start

```bash
git clone https://github.com/Arturo327/R2-Piler
cd R2-Piler
make
./build/r2p --dump-tokens path/to/file.r2
```

Run the test suite:

```bash
make test
```

## Architecture

```
src/
├── main.c           # CLI entry point: arg parsing, orchestration
├── compiler.c/h     # Compiler context, wires the lexer (and future stages) together
├── arena/
|   └── arena.c/h    # Bump-allocator arena; owns all compiler memory
├── error/
|   └── error.c/h    # Error reporting: other stages call it to report an error
└── lexer/
    └── lexer.c/h    # Tokenizer: keywords, literals, operators, error reporting
```

## License

MIT

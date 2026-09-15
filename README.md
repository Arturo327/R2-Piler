# R2-Piler

A small compiler for the (invented) **R2-Lang** language, written in C.

---

## Current Status

Currently implements a complete lexer and parser with arena-based memory management. The type checker, and code generator are not built yet.

**Early stage / work in progress.**

- Lexer — implemented and tested
- Parser — implemented and tested
- Type checker — implemented and tested
- IR — not started
- Code generation — not started

For now, `--dump-tokens`, `--dump-ast` and `--dump-symbols` are the main ways to see the compiler do something.

---

## Features

- Lexer: identifiers, keywords, integer/char/string literals (with escape sequences), `//` comments, and the full set of operators (arithmetic, bitwise, logical, comparison).
- Parser: expresions, variable declarations, blocks, if statements, functions, for and while loops.
- Sema: strict type checking, unitialized and undeclared variable detector, symbol table, allow foward-calls
- Precise error reporting with `file:line:column` locations.
- Arena allocator — all compiler memory is freed in a single call at program exit instead of scattered `malloc`/`free` calls.
- `--dump-tokens` flag to inspect exactly what the lexer produces for a given source file.
- `--dump-ast` flag to inspect exactly the AST produced by the pasrser for a given source file.
- Fixture-based test runner (`make test`) that checks stdout, stderr, and exit status.

---

## Quick Start

```bash
git clone https://github.com/Arturo327/R2-Piler
cd R2-Piler
make
./build/r2p --dump-tokens path/to/file.r2
./build/r2p --dump-ast path/to/file.r2
./build/r2p --dump-symbols path/to/file.r2
```

Run the test suite:

```bash
make test
```

---

## Language

R2-Lang is an invented language, see LANGUAGE.md for more info.

---

## Architecture

```
src/
├── main.c           # CLI entry point: arg parsing, orchestration
├── compiler.c/h     # Compiler context, wires the lexer (and future stages) together
├── arena/
|   └── arena.c/h    # Bump-allocator arena; owns all compiler memory
├── error/
|   └── error.c/h    # Error reporting: other stages call it to report an error
├── parser/
|   ├── ast.h        # AST definition: nodes, types, AST tree
|   └── parser.c/h   # Parser: get the tokens and crate an AST tree
├── sema/
|   ├── symbol.c/h   # Symbol and Symbol table definition
|   └── sema.c/h     # Semantic analyzer: analyze the AST, reports remainig errors and generate symbol table
└── lexer/
    └── lexer.c/h    # Tokenizer: keywords, literals, operators
```

---

## License

MIT

---

# R2-Piler

A small compiler for the (invented) **R2-Lang** language, written in C.

---

## Current Status

The code generator is not built yet, the rest is correctly implemented.

**Work in progress.**

- Lexer — implemented and tested
- Parser — implemented and tested
- Type checker — implemented and tested
- IR — implemented and tested
- Code generation — working on

For now, `--dump-tokens`, `--dump-ast`, `--dump-symbols` and `--dump-ir` are the main ways to see the compiler do something.

---

## Features

- Lexer: identifiers, keywords, integer/char/string literals (with escape sequences), `//` comments, and the full set of operators (arithmetic, bitwise, logical, comparison).
- Parser: expresions, variable declarations, blocks, if statements, functions, for and while loops.
- Sema: strict type checking, unitialized and undeclared variable detector, symbol table, allow foward-calls
- IR: linear per-function stream with virtual registers, short-circuit `&&`/`||`, global init function (`__r2_init`), full lowering of expressions, calls, `if`/`while`/`for` and `return`.
- Precise error reporting with `file:line:column` locations.
- Arena allocator — all compiler memory is freed in a single call at program exit instead of scattered `malloc`/`free` calls.
- `--dump-tokens` flag to inspect exactly what the lexer produces for a given source file.
- `--dump-ast` flag to inspect exactly the AST produced by the pasrser for a given source file.
- `--dump-symbols` flag to inspect the resolved symbol table for a given source file.
- `--dump-ir` flag to inspect the generated IR for a given source file.
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
./build/r2p --dump-ir path/to/file.r2
```

Run the test suite:

```bash
make test
```

This runs the four fixture suites: `test_lexer` (`--dump-tokens` over `tests/lexer/*_src.r2`), `test_parser` (`--dump-ast` over `tests/parser/*_src.r2`), `test_sema` (`--dump-symbols` over `tests/sema/*_src.r2`) and `test_ir` (`--dump-ir` over `tests/ir/*_src.r2`).

---

## Language

R2-Lang is an invented language, see LANGUAGE.md for more info.

---

## Architecture

```
src/
├── main.c           # CLI entry point: arg parsing, orchestration
├── compiler.c/h     # Compiler context, wires lexer + parser + sema + IR together
├── arena/
|   └── arena.c/h    # Bump-allocator arena; owns all compiler memory
├── error/
|   └── error.c/h    # Error reporting: other stages call it to report an error
├── ir/
|   └── ir.c/h       # Intermediate Representation: defines and generates the IR
├── parser/
|   ├── ast.h        # AST definition: nodes, types, AST tree
|   └── parser.c/h   # Parser: get the tokens and crate an AST tree
├── codegen/
|   ├── x86_64.c/h   # x86_64: translates the IR to x86-64 assembly
|   └── codegen.c/h  # Codegen: wires the different architectures and interpreter mode and manage opening/closing files.
├── sema/
|   ├── symbol.c/h   # Symbol and Symbol table definition
|   └── sema.c/h     # Semantic analyzer: analyze the AST, reports remainig errors and generate symbol table
└── lexer/
    └── lexer.c/h    # Tokenizer: keywords, literals, operators
```

Tests live under `tests/`, each fixture as `<name>_src.r2` plus expected `<name>_result.txt` (and optional `<name>_stderr.txt` / `<name>_status.txt`), checked by `tests/run_suite.sh`.

---

## License

MIT

---

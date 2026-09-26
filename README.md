# R2-Piler

A small compiler for the (invented) **R2-Lang** language, written in C.

---

## Current Status

The code generator backend exists but is still a stub: `gen_x86_64` emits nothing, so a plain compile writes an empty `.s` file. The rest is implemented and tested.

**Work in progress.**

- Lexer — implemented and tested
- Parser — implemented and tested
- Type checker — implemented and tested
- IR — implemented and tested
- Code generation — scaffolded (CLI, buffer, backends table); x86-64 emission and the interpreter are still pending

For now, `--dump-tokens`, `--dump-ast`, `--dump-symbols` and `--dump-ir` are the main ways to see the compiler do something.

---

## Features

- Lexer: identifiers, keywords, integer/char/string literals (with escape sequences), `//` comments, and the full set of operators (arithmetic, bitwise, logical, comparison).
- Parser: expressions, variable declarations, blocks, if statements, functions, for and while loops.
- Sema: strict type checking, uninitialized and undeclared variable detector (flow-sensitive definite assignment, uninitialized globals, constant-condition warnings), symbol table, allow forward-calls
- IR: linear per-function stream with virtual registers, short-circuit `&&`/`||`, global init function (`__r2_init`), full lowering of expressions, calls, `if`/`while`/`for` and `return`.
- Precise error reporting with `file:line:column` locations.
- Arena allocator — all compiler memory is freed in a single call at program exit instead of scattered `malloc`/`free` calls.
- `--dump-tokens` flag to inspect exactly what the lexer produces for a given source file.
- `--dump-ast` flag to inspect exactly the AST produced by the parser for a given source file.
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

A plain compile (no dump flag) writes assembly to `out.s` (or the `-o` target). Note: until the x86-64 backend is implemented, the emitted file is empty; `-a arm`, `-a riscv` and `-e` (interpreter) are rejected with "backend is not implemented".

Run the test suite:

```bash
make test
```

This runs the four fixture suites: `test_lexer` (`--dump-tokens` over `tests/lexer/*_src.r2`), `test_parser` (`--dump-ast` over `tests/parser/*_src.r2`), `test_sema` (`--dump-symbols` over `tests/sema/*_src.r2`) and `test_ir` (`--dump-ir` over `tests/ir/*_src.r2`).

`tests/regen_fixtures.sh build/r2p` regenerates every `_stderr.txt` fixture from actual binary output (verifying stdout and exit status did not change); useful whenever diagnostics change.

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
|   ├── x86_64.c/h   # x86_64 backend: stub, does not emit assembly yet
|   └── codegen.c/h  # Codegen: wires the different architectures and interpreter mode and manage opening/closing files.
├── sema/
|   ├── sema.c/h      # Semantic analyzer driver: declarations, global inits, symbol dump
|   ├── common.h      # Private cross-file declarations for sema/
|   ├── expressions.c # Expression checking: ids, calls, assignment, operators
|   ├── literals.c    # Literal folding, flex-literal retagging, casts, unification
|   ├── statements.c  # Statement checking: blocks, if/elif/else, loops, return
|   └── symbol.c/h    # Symbol and Symbol table definition
└── lexer/
    └── lexer.c/h    # Tokenizer: keywords, literals, operators
```

Tests live under `tests/`, each fixture as `<name>_src.r2` plus expected `<name>_result.txt` (and optional `<name>_stderr.txt` / `<name>_status.txt`), checked by `tests/run_suite.sh`.

---

## License

MIT

---

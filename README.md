# R2-Piler

A small compiler for the (invented) **R2-Lang** language, written in C.

---

## Current Status

The x86-64 backend now lowers every IR instruction to AT&T assembly, so a plain compile produces a `.s` file that `gcc` can assemble and run. It is deliberately naive: there is no register allocator yet, every virtual register lives in a stack slot. The front end and the IR are implemented and tested.

**Work in progress.**

- Lexer — implemented and tested
- Parser — implemented and tested
- Type checker — implemented and tested
- IR — implemented and tested
- Code generation — x86-64 implemented (memory-homed registers, System V ABI) and tested with the `tests/asm/` return-code suite; ARM, RISC-V and the interpreter are still pending
- IR optimizer (`optimize_ir`) — pending
- Tests — lexer, parser, sema, IR and asm (x86-64 execution) fixtures

`--dump-tokens`, `--dump-ast`, `--dump-symbols` and `--dump-ir` show what each stage produces; a plain compile now generates runnable assembly.

---

## Features

- Lexer: identifiers, keywords, integer/char/string literals (with escape sequences), `//` comments, and the full set of operators (arithmetic, bitwise, logical, comparison).
- Parser: expressions, variable declarations, blocks, if statements, functions, for and while loops.
- Sema: strict type checking, uninitialized and undeclared variable detector (flow-sensitive definite assignment, uninitialized globals, constant-condition warnings), symbol table, allow forward-calls
- IR: linear per-function stream with virtual registers, short-circuit `&&`/`||`, global init function (`__r2_init`), full lowering of expressions, calls, `if`/`while`/`for` and `return`.
- Codegen: x86-64 AT&T assembly (System V AMD64 ABI). Every virtual register has a stack home sized to its type; operands are loaded with sign/zero extension, arithmetic runs on 64 bits and the store truncates, division runs at the native width, globals are `.data`/`.bss` and RIP-relative. The synthetic `__r2_init` holds global inits (emitted as `__r2.init` only when non-empty, with a dot to avoid colliding with a user global/fn named `init`) and a `main` wrapper calls it before `__r2_main`.
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

A plain compile (no dump flag) writes assembly to `<source>.s` (or the `-o` target; `-` for stdout). Assemble and run it with gcc:

```bash
./build/r2p prog.r2      # writes prog.s
gcc prog.s -o prog
./prog; echo $?          # exit code = value returned by main
```

`-a arm`, `-a riscv` and `-e` (interpreter) are rejected with "backend is not implemented".

Run the test suite:

```bash
make test
```

This runs the five fixture suites: `test_lexer` (`--dump-tokens` over `tests/lexer/*_src.r2`), `test_parser` (`--dump-ast` over `tests/parser/*_src.r2`), `test_sema` (`--dump-symbols` over `tests/sema/*_src.r2`), `test_ir` (`--dump-ir` over `tests/ir/*_src.r2`) and `test_asm` (`tests/run_asm.sh` over `tests/asm/*_src.r2`, which compiles, assembles with `gcc`, runs the binary and checks the exit code in `<name>_exit.txt`).

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
|   ├── x86_64.c/h   # x86_64 backend: lowers the IR to AT&T assembly (System V ABI)
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

Tests live under `tests/`: dump fixtures as `<name>_src.r2` plus expected `<name>_result.txt` (and optional `<name>_stderr.txt` / `<name>_status.txt`), checked by `tests/run_suite.sh`; assembler fixtures as `tests/asm/<name>_src.r2` plus expected `<name>_exit.txt` (and optional `<name>_stderr.txt`), checked by `tests/run_asm.sh` (compile + `gcc` + run, compare exit code).

---

## License

MIT

---

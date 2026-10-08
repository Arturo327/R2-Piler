# R2-Piler

A small compiler for the (invented) **R2-Lang** language, written in C.

---

## Current Status

The x86-64 backend lowers every IR instruction to AT&T assembly, so a plain compile produces a `.s` file that `gcc` can assemble and run. It does peephole codegen opts (see `comp_pending.md`: deferred single-use stores, const/alias propagation, read-modify-write, mul/div/mod/shift strength reduction incl. signed-pow2 and magic division, cmp+jcc fusion, DCE, tail calls); there is no register allocator yet, surviving virtual registers live in stack slots. The front end, the IR and the IR optimizer are implemented and tested.

**Work in progress.**

- Lexer — implemented and tested
- Parser — implemented and tested
- Type checker — implemented and tested
- IR — implemented and tested
- Code generation — x86-64 implemented (memory-homed registers, System V ABI) and tested with the `tests/asm/` return-code suite; ARM, RISC-V and the interpreter are still pending
- IR optimizer (`optimize_ir`) — implemented and tested (`--dump-opt`, suite `tests/opt/`)
- Tests — lexer, parser, sema, IR, opt and asm (x86-64 execution) fixtures

`--dump-tokens`, `--dump-ast`, `--dump-symbols`, `--dump-ir` and `--dump-opt` show what each stage produces; a plain compile now generates runnable assembly.

---

## Features

- Lexer: identifiers, keywords, integer/char/string literals (with escape sequences), `//` comments, and the full set of operators (arithmetic, bitwise, logical, comparison).
- Parser: expressions, variable declarations, blocks, if statements, functions, for and while loops.
- Sema: strict type checking, uninitialized and undeclared variable detector (flow-sensitive definite assignment, uninitialized globals, constant-condition warnings), symbol table, allow forward-calls
- IR: linear per-function stream with virtual registers, short-circuit `&&`/`||`, global init function (`__r2_init`), full lowering of expressions, calls, `if`/`while`/`for` and `return`.
- IR optimizer: architecture-independent passes over the linear IR (copy/const propagation, constant folding incl. constant jumps, `ADD;MOVE` coalescing at `-O2`, dead-code elimination, unreachable-block pruning, jump cleanup). Runs by default at `-O1` before codegen (`-O0` disables it, `-O2` enables the full set); `--dump-opt` prints the optimized IR (defaults to full optimization unless `-O` is given).
- Codegen: x86-64 AT&T assembly (System V AMD64 ABI). Surviving virtual registers have sized stack homes; operands load with sign/zero extension, arithmetic runs on 64 bits and the store truncates, division runs at the native width (except constant divisors, lowered via shifts/magic), globals are `.data`/`.bss` and RIP-relative. Peephole opts: single-use `i64` temporaries stay in `%rax` (no slot/store), single-def `MOVE`s propagate consts/aliases as immediates/slot reuse, `x = x op K` becomes one RMW mem op, `*2^k`/`*3,5,9`/`*(2^k±1)` → `shl`/`lea`, `/`/`%` by const → pow2 `shr`/`and` (signed via bias) or magic `mul`+shifts, shifts take immediates, `cmp` fuses with the next `jz`/`jnz`, unused pure defs are skipped, `return f()` with ≤6 args becomes `leave; jmp`. The synthetic `__r2_init` holds global inits (emitted as `__r2.init` only when non-empty, with a dot to avoid colliding with a user global/fn named `init`) and a `main` wrapper calls it before `__r2_main`.
- Precise error reporting with `file:line:column` locations.
- Arena allocator — all compiler memory is freed in a single call at program exit instead of scattered `malloc`/`free` calls.
- `--dump-tokens` flag to inspect exactly what the lexer produces for a given source file.
- `--dump-ast` flag to inspect exactly the AST produced by the parser for a given source file.
- `--dump-symbols` flag to inspect the resolved symbol table for a given source file.
- `--dump-ir` flag to inspect the generated IR for a given source file.
- `--dump-opt` flag to inspect the optimized IR for a given source file.
- `-O0` / `-O1` / `-O2` (or `--opt[=LEVEL]`) to select the optimization level (default: `1`).
- Fixture-based test runner (`make test`) that checks stdout, stderr, and exit status.

---

## Quick Start

```bash
git clone https://github.com/Arturo327/R2-Piler
cd R2-Piler
make
./build/r2p prog.r2      # writes prog.s
gcc prog.s -o prog
./prog; echo $?          # exit code = value returned by main
```

A plain compile (no dump flag) writes assembly to `<source>.s` (or the `-o` target; `-` for stdout). Assemble with gcc and run it.

`-a arm`, `-a riscv` and `-e` (interpreter) are rejected with "backend is not implemented".

Run the test suite:

```bash
make test
```

This runs the six fixture suites: `test_lexer` (`--dump-tokens` over `tests/lexer/*_src.r2`), `test_parser` (`--dump-ast` over `tests/parser/*_src.r2`), `test_sema` (`--dump-symbols` over `tests/sema/*_src.r2`), `test_ir` (`--dump-ir` over `tests/ir/*_src.r2`), `test_opt` (`--dump-opt` over `tests/opt/*_src.r2`) and `test_asm` (`tests/run_asm.sh` over `tests/asm/*_src.r2`, which compiles, assembles with `gcc`, runs the binary and checks the exit code in `<name>_exit.txt`; each test is also recompiled with `-O0` and `-O2` and must exit with the same code).

`tests/regen_fixtures.sh build/r2p` regenerates every `_stderr.txt` fixture from actual binary output (verifying stdout and exit status did not change); useful whenever diagnostics change.

Some debug flags:
```bash
./build/r2p --dump-tokens path/to/file.r2
./build/r2p --dump-ast path/to/file.r2
./build/r2p --dump-symbols path/to/file.r2
./build/r2p --dump-ir path/to/file.r2
./build/r2p --dump-opt path/to/file.r2
```

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
|   ├── ir.c/h       # Intermediate Representation: defines and generates the IR
|   └── opt.c/h      # IR optimizer: propagate/fold/coalesce/DCE/unreachable/jumps + --dump-opt
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

Tests live under `tests/`: dump fixtures as `<name>_src.r2` plus expected `<name>_result.txt` (and optional `<name>_stderr.txt` / `<name>_status.txt`), checked by `tests/run_suite.sh` (lexer, parser, sema, ir and opt suites; opt uses `--dump-opt`); assembler fixtures as `tests/asm/<name>_src.r2` plus expected `<name>_exit.txt` (and optional `<name>_stderr.txt`), checked by `tests/run_asm.sh` (compile + `gcc` + run, compare exit code, plus `-O0`/`-O2` differential).

---

## License

MIT

---

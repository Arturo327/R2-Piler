# R2-Piler

A small compiler for the (invented) **R2-Lang** language, written in C.

---

## Current Status

The x86-64 backend lowers every IR instruction to AT&T assembly, so a plain compile produces a `.s` file that `gcc` can assemble and run. It does peephole codegen opts (deferred single-use stores, const/alias propagation, read-modify-write incl. inplace unary, mul/div/mod/shift strength reduction incl. signed-pow2 bias + magic division, cmp+jcc fusion, local DCE + dead code after `RET`/`JMP`, tail calls, `rax` caching); there is no register allocator yet, surviving virtual registers live in stack slots. The front end, the IR and the 11-pass IR optimizer are implemented and tested.

**Work in progress.**

- Lexer — implemented and tested
- Parser — implemented and tested
- Type checker — implemented and tested
- IR — implemented and tested
- Code generation — x86-64 implemented (memory-homed registers, System V ABI) and tested with the `tests/asm/` return-code suite (51 fixtures); ARM, RISC-V and the interpreter are still pending
- IR optimizer (`optimize_ir`) — implemented and tested (`--dump-opt`, suite `tests/opt/`, 15 fixtures: `001`-`006` plus `007_simplify`, `008_fold_const`, `009_cse_mul`, `010_live_dce`, `011_strength`, `012_globals_fwd`, `013_dse`, `014_static_init`, `015_thread_jump`)
- Tests — 5 dump suites via `make test` (lexer, parser, sema, ir, opt); asm execution + callgrind bench via `make bench`

`--dump-tokens`, `--dump-ast`, `--dump-symbols`, `--dump-ir` and `--dump-opt` show what each stage produces; a plain compile now generates runnable assembly.

---

## Features

- Lexer: identifiers, keywords, integer/char/string literals (with escape sequences), `//` comments, and the full set of operators (arithmetic, bitwise, logical, comparison).
- Parser: expressions, variable declarations, blocks, if statements, functions, for and while loops.
- Sema: strict type checking, uninitialized and undeclared variable detector (flow-sensitive definite assignment, uninitialized globals, constant-condition warnings), symbol table, allow forward-calls
- IR: linear per-function stream with virtual registers, short-circuit `&&`/`||`, global init function (`__r2_init`), full lowering of expressions, calls, `if`/`while`/`for` and `return`.
- IR optimizer: 11 architecture-independent passes over the linear IR in `passes[]` order (`src/ir/opt.c:29-41`): propagate (BASIC), fold (BASIC, no fold of div/mod by zero, `INT64_MIN` div/mod `-1`, shifts `>= 64`), cse (FULL only: `MUL`/`DIV`/`MOD` with non-const src2, swap only `MUL`, cap 32), coalesce (BASIC), dce (BASIC, refcount), live_dce (FULL, CFG liveness), unreachable (BASIC, keeps last instr), jumps (BASIC: threading up to 8 hops + `flip_branch` + `jump_to_next` + unreferenced labels), strength (BASIC: `MUL`/`DIV`/`MOD` by pow2 `>= 2`, `DIV`/`MOD` unsigned only), globals (BASIC intra-block forwarding, `CALL` invalidates), dse (BASIC), plus `static_init` always (when level != `NO_OPT` and globals exist; interprocedural `gtouch`, `CONST`+`STR` bind of same size/type feeds `.data`). Fixpoint up to 8 rounds, `analyze_fn` only when dirty, `compact_ir` without renumbering (`N regs` headers keep the original count). Runs by default at `-O1` before codegen (`-O0` disables it, `-O2` enables the full set); `--dump-opt` prints the optimized IR (forces `FULL` unless `-O` is given; `--dump-opt -O0` shows unoptimized; `--dump-ir` never optimizes). Extra: `simplify` algebraics (`x-x`/`x^x`/`x!=x`/`x<x`/`x>x`→`0`, `x==x`/`x<=x`/`x>=x`→`1`, `+0`/`-0`/`|0`/`^0`/`<<0`/`>>0`→`MOVE`, `/1`→`MOVE`, `%1`→`0`, `*0`→`0`, `*1`→`MOVE`, `&0`→`0`, `&all1`→`MOVE`, const swapped right).
- Codegen: x86-64 AT&T assembly (System V AMD64 ABI). Surviving virtual registers have sized stack homes; operands load with sign/zero extension, arithmetic runs on 64 bits and the store truncates, division runs at the native width (except constant divisors, lowered via shifts/magic), globals are `.data`/`.bss` via the IR `has_init` flag (no textual scan) and RIP-relative. Peephole opts: single-use 8-byte prods stay in `%rax` as deferred `VR_REG` (no slot/store), single-def `MOVE`s propagate consts/aliases as immediates/slot reuse (`VR_CONST`/`VR_ALIAS` via `same_rep`), `x = x op K` becomes one RMW mem op (3-instr pattern plus inplace `add`/`sub`/`and`/`or`/`xor`/`shl`/`sar`/`shr`/`neg`/`not`), `*2^k`→`shl`, `*3/5/9`→`lea`, `*(2^k±1)`→`shl+add/sub`, `/`/`%` by const → pow2 `shr`/`and` (signed via bias) or magic `mulq`/`imulq`+shifts (bails on `v<=0`, signed `/1`), shifts take immediates else `%cl`, `cmp` fuses with the next `jz`/`jnz` (plus `cmp_mem_imm` and `op_rax` fast paths), unused pure defs are skipped (local `is_silent` DCE, incl. dead code after `RET`/`JMP`), `return f()` with ≤6 args and same/8-byte ret becomes `leave; jmp` (self-call → `jmp .LSn`), `%rax` value caching (`rax_v`, `testq %rax` fast path). Frames are `align16(outgoing + slots)` with slots grouped 8/4/2/1, leaf fns ≤128 bytes stay in the red zone with no prologue, stack args live in an outgoing area of `8*max(argc-6)` excluding tail calls. The synthetic `__r2_init` holds global inits (emitted as `__r2.init` only when non-empty, with a dot to avoid colliding with a user global/fn named `init`) and a `main` wrapper calls it before `__r2_main`.
- Precise error reporting with `file:line:column` locations.
- Arena allocator — all compiler memory is freed in a single call at program exit instead of scattered `malloc`/`free` calls.
- `--dump-tokens` flag to inspect exactly what the lexer produces for a given source file.
- `--dump-ast` flag to inspect exactly the AST produced by the parser for a given source file.
- `--dump-symbols` flag to inspect the resolved symbol table for a given source file.
- `--dump-ir` flag to inspect the generated IR for a given source file.
- `--dump-opt` flag to inspect the optimized IR for a given source file.
- `-O0` / `-O1` / `-O2` (or `--opt[=LEVEL]`) to select the optimization level (default: `1`; `-O0` = `NO_OPT`, `-O1` = `BASIC`, `-O2` = `FULL`).
- Fixture-based test runner (`make test`, 5 dump suites) plus asm bench runner (`make bench`, exit codes + callgrind Ir table).

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

Run the test suites:

```bash
make test    # 5 dump suites: lexer, parser, sema, ir, opt (opt always FULL)
make bench   # asm suite: compiles at default O1 + -O0/-O1/-O2 loop, gcc + run,
             # exit code must match; valgrind callgrind counts Ir per level
```

`make test` runs `test_lexer` (`--dump-tokens` over `tests/lexer/*_src.r2`), `test_parser` (`--dump-ast` over `tests/parser/*_src.r2`), `test_sema` (`--dump-symbols` over `tests/sema/*_src.r2`), `test_ir` (`--dump-ir` over `tests/ir/*_src.r2`) and `test_opt` (`--dump-opt` over `tests/opt/*_src.r2`, 15 fixtures, always `FULL`). `make bench` runs `tests/run_asm.sh` over `tests/asm/*_src.r2` (51 fixtures): compiles each (default `-O1`), assembles with `gcc`, runs the binary and checks the exit code in `<name>_exit.txt`; each test is also recompiled with `-O0`/`-O1`/`-O2` under valgrind callgrind (`--toggle-collect=__r2_main,__r2.init`, user-code Ir only) and must exit with the same code — the run only fails on exit mismatch, printing an `O0/O1/O2 Ir + TOTAL` table (approx totals: O0 ~1.75B, O1 ~1.02B, O2 ~1.00B Ir). O1 vs O2 differ only on `009_cse_mul` and `010_live_dce` (the two `FULL`-only fixtures).

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
|   ├── opt.h        # IR optimizer: only optimize_ir + OptFn/Optimizer structs
|   ├── common.h     # Private cross-file declarations for ir/ (like sema/common.h)
|   ├── opt.c                # Orchestrator (142 lines): passes[], optimize_fn,
|   │                        # compact_ir, init_caches, init_opt, optimize_ir
|   └── opts/                # One .c per pass group, sharing common.h (no externs)
|       ├── analyze.c        # fill_blocks/build_lmap/count_regs/count_label_refs/
|       │                    # analyze_fn/label_block/build_cfg/mark_reachable
|       ├── utils.c          # kill_instr/next_real/same_rep/norm_val
|       ├── propagate.c      # copy/const propagation (BASIC)
|       ├── fold.c           # reg_const/eval_*/fold_instr/fold_jump/track_const/
|       │                    # fold_block/opt_fold (BASIC)
|       ├── simplify.c       # simplify_same/rhs/lhs/swap_op/simplify_instr (BASIC)
|       ├── cse_coalesce.c   # cse (FULL) + coalesce (BASIC)
|       ├── dce_live.c       # opt_dce (BASIC) + live_* + opt_live_dce (FULL)
|       ├── cfg.c            # unreachable (BASIC) + thread_jump/flip_branch/
|       │                    # jump_to_next (BASIC)
|       ├── extra.c          # strength/globals/dse (BASIC)
|       └── static.c         # static_init con gtouch interprocedural (always)
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

Source is auto-discovered (`find src -name '*.c'` in the Makefile); no new `.h` was added for the optimizer split. Tests live under `tests/`: dump fixtures as `<name>_src.r2` plus expected `<name>_result.txt` (and optional `<name>_stderr.txt` / `<name>_status.txt`), checked by `tests/run_suite.sh` (lexer, parser, sema, ir and opt suites; opt uses `--dump-opt`, always `FULL`); assembler fixtures as `tests/asm/<name>_src.r2` plus expected `<name>_exit.txt` (and optional `<name>_stderr.txt`), checked by `tests/run_asm.sh` (default `-O1` compile + `-O0`/`-O1`/`-O2` loop with `gcc` + valgrind callgrind Ir counts, compare exit code, fail only on exit mismatch).

---

## License

MIT

---

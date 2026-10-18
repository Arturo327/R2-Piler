# R2-Piler

A small compiler for the (invented) **R2-Lang** language, written in C.

---

## Features

- A small, statically and strongly typed imperative language: eight integer types (`i8` to `u64`, with `char` as an alias of `i8`), explicit casts with `as`, and implicit widening only when no value can be lost.
- Full statement set: `var` declarations, blocks, `if`/`elif`/`else`, `while`, `for` and `return`, plus top-level functions with parameters by value, recursion and forward references.
- Top-level globals with well-defined initialization order; cyclic initializers are diagnosed.
- Short-circuit `&&` and `||`.
- Strict diagnostics with `file:line:column` locations: uninitialized-variable warnings, missing-`return` checks and constant conditions.
- Compiles to x86-64 AT&T assembly (System V AMD64 ABI) that links with `gcc`.
- Four optimization levels: `-O0` to `-O3` (default `-O2`).
- Stage-inspection flags: `--dump-tokens`, `--dump-ast`, `--dump-symbols`, `--dump-ir` and `--dump-opt`.

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

R2-Lang is an invented language; see LANGUAGE.md for the full specification and compiler usage.

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
|   ├── opt.c                # Orchestrator: passes[], optimize_fn,
|   │                        # compact_ir, init_caches, init_opt, optimize_ir
|   └── opts/                # One .c per pass group, sharing common.h (no externs)
|       ├── common.h         # Private cross-file declarations for ir/opts
|       ├── analyze.c        # analyze_fn: blocks/CFG + reg def-use + label refs
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
|       ├── inline.c         # inline_ir (FULL only, re-optimizes afterwards)
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

Source is auto-discovered (`find src -name '*.c'` in the Makefile).

---

## License

MIT

---

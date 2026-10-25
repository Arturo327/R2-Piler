# R2-Piler

A small compiler for the (invented) **R2-Lang** language, written in C.

R2-Piler takes a `.r2` source file through the whole pipeline — lexer, parser,
semantic analysis, IR, optimizer and x86-64 code generation — and writes AT&T
assembly. The result links with `gcc`; there is no runtime library to ship.

---

## Status

R2-Piler is a work in progress, but the implemented subset is usable and well
tested:

- **187 dump fixtures** pass (`make test`): 41 lexer, 45 parser, 55 sema,
  27 IR and 19 optimizer tests.
- **70 executable assembly fixtures** pass (`make bench`) at `-O0`, `-O1`,
  `-O2` and `-O3`, including register-allocation and ABI stress tests.
- The only working backend is **x86-64** (System V AMD64 ABI). ARM, RISC-V and
  the interpreter are recognized on the command line but rejected as
  “backend is not implemented”.
- The language currently has integers, functions, locals, globals, blocks,
  `if`/`elif`/`else`, `while`, `for`, `return`, casts, short-circuit logic and
  the full integer/bitwise operator set.
- Pointers, arrays, strings, structs, enums, modules/imports, classes,
  floating point and compound assignment are **not implemented yet**. See
  [Roadmap](#roadmap).

---

## Features

- A small, statically and strongly typed imperative language: eight integer
  types (`i8` to `u64`, with `char` as an alias of `i8`), explicit casts with
  `as`, and implicit widening only when no value can be lost.
- Full statement set: `var` declarations, blocks, `if`/`elif`/`else`, `while`,
  `for` and `return`, plus top-level functions with parameters by value,
  recursion and forward references.
- Top-level globals with well-defined initialization order; cyclic initializers
  are diagnosed.
- Short-circuit `&&` and `||`.
- Strict diagnostics with `file:line:column` locations: uninitialized-variable
  warnings, missing-`return` checks and constant conditions.
- Compiles to x86-64 AT&T assembly (System V AMD64 ABI) that links with `gcc`.
- Four optimization levels: `-O0` to `-O3` (default `-O2`).
- Stage-inspection flags: `--dump-tokens`, `--dump-ast`, `--dump-symbols`,
  `--dump-ir` and `--dump-opt`.

---

## Requirements

- A C compiler (the Makefile defaults to `gcc`).
- `make`.
- `valgrind` in `PATH` only if you want to run `make bench` (the assembly suite
  uses callgrind to report deterministic instruction counts).

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

A plain compile (no dump flag) writes assembly to `<source>.s` (or the `-o`
target; `-` for stdout). Assemble with gcc and run it.

A minimal program:

```r2
fn main () : i64
{
	return 42;
}
```

Some debug flags:

```bash
./build/r2p --dump-tokens path/to/file.r2
./build/r2p --dump-ast path/to/file.r2
./build/r2p --dump-symbols path/to/file.r2
./build/r2p --dump-ir path/to/file.r2
./build/r2p --dump-opt path/to/file.r2
```

`--dump-ir` never runs the optimizer and never shows `select`.
`--dump-opt` without `-O` forces the maximum (`-O3`) level; with `-O0`/`-O1`
it prints the unoptimized IR.

---

## Language

R2-Lang is an invented language; see [LANGUAGE.md](LANGUAGE.md) for the full
specification, the grammar, the semantics of each type/operator and the
current limitations.

---

## Architecture

```
src/
├── main.c           # CLI entry point: argument parsing, orchestration
├── compiler.c/h     # Compiler context, wires lexer + parser + sema + IR + codegen
├── arena/
│   └── arena.c/h    # Bump-allocator arena; owns all compiler memory
├── error/
│   └── error.c/h    # Diagnostics with file:line:column locations
├── lexer/
│   └── lexer.c/h    # Tokenizer: keywords, literals, operators, comments
├── parser/
│   ├── ast.h        # AST definition: nodes, types, source spans
│   ├── parser.c     # Recursive-descent parser
│   └── parser.h     # Parser state and entry points
├── sema/
│   ├── sema.c/h     # Semantic driver: declarations, global inits, symbol dump
│   ├── common.h     # Private cross-file declarations for sema/
│   ├── expressions.c # Expression checking: ids, calls, assignment, operators
│   ├── literals.c   # Literal folding, flexible literals, casts, unification
│   ├── statements.c # Blocks, if/elif/else, loops, return, main signature
│   └── symbol.c/h   # Symbol and symbol-table definitions
├── ir/
│   ├── ir.c/h       # IR definition, generation and dump
│   ├── cfg.c/h      # Shared CFG + liveness analysis (optimizer and regalloc)
│   ├── opt.h        # Optimizer API: OptLevel, OptFn/Optimizer, CseEntry
│   ├── opt.c        # Orchestrator: passes[], optimize_fn, compact_ir,
│   │                # init_caches, init_opt, optimize_ir
│   └── opts/        # One .c per pass group, sharing common.h
│       ├── common.h # Private cross-file declarations for ir/opts
│       ├── analyze.c # analyze_fn: blocks/CFG + reg def-use + label refs
│       ├── utils.c  # kill_instr/next_real/same_rep/norm_val
│       ├── propagate.c # Copy/constant propagation (BASIC)
│       ├── fold.c   # Constant folding, jump folding, track_const (BASIC)
│       ├── simplify.c # Algebraic simplification and operand swapping (BASIC)
│       ├── cse_coalesce.c # CSE (FULL) + coalesce (BASIC)
│       ├── dce_live.c # Local DCE (BASIC) + global live DCE (FULL)
│       ├── cfg.c    # Unreachable blocks, jump threading/flip (BASIC)
│       ├── extra.c  # Strength reduction, global forwarding, DSE (BASIC)
│       ├── inline.c # Small leaf-function inlining (FULL, re-optimizes)
│       └── static.c # Static .data binding for constant global inits
├── codegen/
│   ├── codegen.c/h  # Backend table, output buffer, shared codegen helpers
│   ├── parallel.c/h # Parallel-move planner used by PARAM/ARG and edge moves
│   ├── regalloc/
│   │   ├── regalloc.c/h # Regalloc driver, contract types, edge-move builder
│   │   ├── common.h     # Graph/coloring structures shared by regalloc files
│   │   ├── color.c      # Loop weights + per-partition coloring driver
│   │   ├── graph.c      # Interference graph, block reservations, scratch search
│   │   └── paint.c      # Graph simplification/coloring and spill selection
│   └── x86-64/
│       ├── common.h   # Shared X86Fn state, masks, cross-file declarations
│       ├── x86_64.c/h # Backend entry point (gen_x86_64) and op needs table
│       ├── layout.c   # Frame layout + regalloc wiring
│       ├── edges.c    # Edge moves and .LE stubs
│       ├── regs.c     # Register helpers + regalloc contract checks
│       ├── mem.c      # Loads/stores, register names, scratch allocation
│       ├── globals.c  # .data/.bss emission and global loads/stores
│       ├── move.c     # PARAM/MOVE emission and parallel moves
│       ├── arith.c    # Arithmetic, shifts, unary ops
│       ├── divmod.c   # Constant div/mod lowering
│       ├── cmp.c      # Comparisons, select, jumps
│       ├── flow.c     # Calls, arguments, returns, tail calls
│       ├── rmw.c      # Read-modify-write fusion
│       └── func.c     # Per-function instruction dispatch/body
└── ...
```

Source is auto-discovered (`find src -name '*.c'` in the Makefile); there is no
list of source files to maintain.

### Pipeline

```
source -> lexer -> parser -> sema -> IR -> optimize_ir (-O2/-O3) -> x86-64 asm
```

An error in a stage stops the compilation before the next stage; warnings do
not. The IR optimizer only runs at `-O2`/`-O3`; `-O1` keeps the IR unoptimized
but runs register allocation, and `-O0` spills every value to the stack.

---

## Roadmap

The detailed plan lives in `comp_pending.md`. The short version, in dependency
order:

1. Compound assignment (`+=`, `-=`, …) and statement-only `++`/`--`; enums.
2. Pointers (`ptr`, `@`, `$`, `NULL`, `sizeof`, C-style pointer arithmetic) and
   string literals in `.rodata`; non-variadic `extern` C calls.
3. Stack arrays, `arr.len`, the `for (x : arr)` desugaring and 16-byte slices.
4. C-style `struct`s with exhaustive literals and `Copy` semantics.
5. `pub`, modules/`import`/`from`, name mangling and `.r2i` interface files.
6. Function-local `static`, `export`/`extern` completion and C header output.
7. Optional arguments, multiple return values, classes/`init`/`op`/`destroy`,
   minimal ownership with automatic `destroy`/`defer`.
8. Floating point (`f32`/`f64`) as a separate backend effort.

Register-allocation and backend improvements are tracked at the top of
`comp_pending.md` (per-instruction reservations, precoloring hints, better edge
moves, spill-slot reuse/rematerialization, stack-argument tail calls, lazy
narrow extension, CFI/frame-pointer elimination, and more).

---

## License

MIT

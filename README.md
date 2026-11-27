# R2-Piler

A small compiler for the (invented) **R2-Lang** language, written in C.

R2-Piler takes a `.r2` source file through the whole pipeline — lexer, parser,
semantic analysis, IR, optimizer and x86-64 code generation — and writes AT&T
assembly. The result links with `gcc`; there is no runtime library to ship.

---

## Status

R2-Piler is a work in progress, but the implemented subset is usable and well
tested: `make test` passes 619 fixtures and `make bench` passes 189 executable
assembly fixtures at `-O0` through `-O3`.

The only working backend is **x86-64** (System V AMD64 ABI); ARM, RISC-V and
the interpreter are not implemented.

See **Features** below for a language overview, and [LANGUAGE.md](LANGUAGE.md)
for the full specification.

Not implemented yet: floating point, `defer` and automatic destruction at
scope exit.

---

## Features

- A small, statically and strongly typed imperative language: eight integer
  types (`i8` to `u64`, with `char` as an alias of `i8`), explicit casts with
  `as`, and implicit widening only when no value can be lost.
- Full statement set: `var` declarations, blocks, `if`/`elif`/`else`, `while`,
  `for`, `switch` and `return`, plus top-level functions with parameters by
  value, recursion and forward references. `static var`/`static arr` locals
  keep one instance with static storage duration and a constant initializer.
- Pointers: `ptr T` and `ptr void`, `@` dereference, `$` address-of, `NULL`,
  `sizeof`, C-like pointer arithmetic and comparison, explicit pointer casts,
  and string literals (usable wherever `ptr i8` is expected).
- Function pointers: `ptr fn(a : i32, b : i32) : i32` (bare `fn(...)` is the
  same type), with signatures checked on initialization, assignment, arguments
  and returns.
- Arrays and slices of any rank: `arr a : T[N]` with `{...}` and string
  initializers, designated initializers, element-scaled indexing, `.len`,
  `sizeof`, decay to `ptr T`, slice parameters `T[]` and fixed-size array
  parameters `T[N]`. The index form of `for` iterates over any number of
  dimensions.
- Structs and unions with C-like layout: designated `{.field = ...}`
  initializers (omitted fields are zero-filled), field access with auto-deref
  on pointers, by-value copies, arguments and returns, arrays of structs,
  globals and `static` structs, and `sizeof`.
- Classes: `class C { x : i64; init(self, ...) {...} fn m(self, ...) {...} }`.
  Methods take an implicit `self : ptr C`; `p.m(...)` inserts the receiver and
  `C::m(...)` calls directly. `priv` members are visible only inside the
  class. A class may define an initializer, invoked with `C(...)` or
  `new C(...)`, and an optional destructor, invoked through `drop`, `reset`
  and `delete`. `new(slot) C(...)` constructs into caller-provided memory.
  Operator overloads are declared with `op` (`op + (self, b : C) : C`,
  unary `op - (self)`, and `op [] (self, i : i64) : ptr T`); they apply to
  class values and a `ptr C` keeps native pointer arithmetic/comparison.
  Classes may be `pub` (they go into the `.r2i` interface, so other R2
  modules can construct, copy, call methods/operators and `drop` them) or
  `export` (the C header gets the plain struct plus a C++ class whose inline
  methods call the mangled `extern "C"` functions).
- Heap allocation: `new T[N]` / `new T[D1][D2]...` (slices) and `new T`
  (a `ptr T`), with optional custom allocators `new(allocator_expr) T[N]`;
  `delete x;` and `delete(expr) x;` free them. The default forms use
  `malloc`/`free` from `import "stdlib";`; memory is not zero-initialized.
- Modules and imports: `import "m";` exposes `m::name`, `import "m" as x;`
  renames the qualifier, and `from "m" import a as b, T;` brings names into
  scope. Only public declarations are visible; imported interfaces are read
  from `.r2i` files and transitive dependencies are loaded automatically.
- Visibility: top-level names are file-private by default; `pub` makes them
  public and `export` gives them a plain C-linkable name. `main` must be
  `pub fn main() : i64`.
- C interop: `extern fn` / `extern var` declarations refer to plain C symbols.
- Short-circuit `&&` and `||`, the ternary `cond ? a : b`, and strict
  diagnostics with `file:line:column` locations.
- Compiles to x86-64 AT&T assembly (System V AMD64 ABI) that links with `gcc`.
  Four optimization levels (`-O0` to `-O3`, default `-O2`) and stage-inspection
  flags `--dump-tokens`, `--dump-ast`, `--dump-symbols`, `--dump-ir` and
  `--dump-opt`.

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

A plain compile writes assembly to `<source>.s` (or the `-o` target; `-` for
stdout) and the module interface `modules/<module>.r2i` used by importers
(`-n`/`--no-r2i` skips it). Modules are compiled one at a time and linked
afterwards:

```bash
./build/r2p a.r2 -m modules      # writes a.s and modules/a.r2i
./build/r2p b.r2 -m modules      # b.r2 has `import "a";`
gcc a.s b.s -o prog
```

A minimal program:

```r2
pub fn main () : i64
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

`--dump-ir` shows the unoptimized IR; `--dump-opt` without `-O` uses the
maximum optimization level.

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
│   ├── parser.h     # Parser state and entry points
│   ├── common.h     # Private cross-file declarations for parser/
│   ├── parser.c     # Driver: token cursor, panic sync, node helpers, parse()
│   ├── expressions.c # Precedence, primaries, calls, casts, unary, ++/--
│   ├── types.c      # Type syntax: parse_type, sizeof, pointer types
│   ├── statements.c # Blocks, if/elif/else, while, for, return, var decls
│   ├── declarations.c # fn/extern/class decls, enums, type/newtype declarations
│   ├── operators.c/h # Class operator canons and source spellings
│   └── dump.c       # AST dump for --dump-ast
├── sema/
│   ├── sema.c/h     # Semantic driver: declarations, global inits, symbol dump
│   ├── common.h     # Private cross-file declarations for sema/
│   ├── expr_internal.h # Private expression/statement declarations for sema/
│   ├── expr_core.c  # Expression checking core
│   ├── expr_vars.c  # Variable/reference checking
│   ├── expr_access.c # Field access, indexing, deref, address, sizeof
│   ├── expr_ops.c   # Operator checking
│   ├── literals.c   # Literal folding, flexible literals, casts, unification
│   ├── statements.c # Blocks, if/elif/else, loops, return, main signature
│   ├── types.c      # Enum/type/newtype declaration resolution
│   ├── structs.c    # Struct/union declaration, layout, initializers
│   ├── agg.c        # Aggregate helpers
│   └── symbol.c/h   # Symbol and symbol-table definitions
├── imports/
│   ├── imports.h    # Module/item/binding records
│   ├── internal.h   # Loader structs and cross-file helpers
│   ├── load.c       # Module lookup, .r2i header, aliases/bindings
│   ├── parse.c      # .r2i parser driver + symbol-name decoding
│   └── lookup.c     # Qualified/from lookup used by sema
├── r2i/
│   ├── r2i.c/h      # Output buffer, header, write/ownership checks
│   ├── decl.c/type.c/expr.c # Interface body
│   └── imports.c    # `imports:` dependency collection
├── header/
│   ├── header.c/h   # C/C++ header output and ownership checks
│   ├── decl.c/type.c # Exported declarations and types
│   └── internal.h   # Private cross-file declarations for header/
├── mangle/
│   └── mangle.c/h   # Module path resolution and symbol mangling
├── ir/
│   ├── core.c/h     # IR definition, module/fn skeleton
│   ├── internal.h   # Private cross-file declarations for ir/
│   ├── stmt.c       # IR generation for statements/params
│   ├── expr.c       # IR generation for expressions (incl. struct copies/sret)
│   ├── expr_access.c # IR helpers for field/index/pointer access
│   ├── expr_assign.c # IR generation for assignment/compound/multi
│   ├── expr_call.c  # IR generation for calls
│   ├── expr_agg.c   # IR generation for aggregates/struct init
│   ├── abi.c        # ABI helpers
│   ├── escape.c     # Escape analysis
│   ├── fuse.c       # Fusion helpers
│   ├── dump.c       # IR dump for --dump-ir/--dump-opt
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
│       ├── disp.c   # Displacement folding
│       ├── fields.c # Field forwarding
│       ├── sroa.c   # Small-struct SROA
│       └── static.c # Static .data binding for constant global inits
├── codegen/
│   ├── codegen.c/h  # Backend table, output buffer, shared codegen helpers
│   ├── parallel.c/h # Parallel-move planner used by PARAM/ARG and edge moves
│   ├── regalloc/
│   │   ├── regalloc.c/h # Regalloc driver, contract types, edge-move builder
│   │   ├── common.h     # Graph/coloring structures shared by regalloc files
│   │   ├── color.c      # Loop weights + per-partition coloring driver
│   │   ├── graph.c      # Interference graph, block reservations, scratch search
│   │   ├── paint.c      # Graph simplification/coloring and spill selection
│   │   ├── shrink.c    # Shrink-wrapping of callee-saved registers
│   │   └── verify.c    # Generic regalloc contract checks (filename as-is)
│   └── x86-64/
│       ├── common.h   # Shared X86Fn state, masks, cross-file declarations
│       ├── x86_64.c/h # Backend entry point (gen_x86_64)
│       ├── layout.c   # Frame layout + regalloc wiring + op needs table
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
│       ├── ptr.c      # Address-of/deref loads/stores + .rodata strings
│       └── func.c     # Per-function instruction dispatch/body
└── ...
```

Source is auto-discovered (`find src -name '*.c'` in the Makefile); there is no
list of source files to maintain.

### Pipeline

```
source -> lexer -> parser -> imports (.r2i) -> sema -> IR -> optimize_ir (-O2/-O3) -> x86-64 asm
```

An error in a stage stops compilation before the next one; warnings do not.
The optimizer runs at `-O2`/`-O3`; `-O1` keeps the IR unoptimized but allocates
registers, and `-O0` spills every value to the stack.

---

## License

MIT

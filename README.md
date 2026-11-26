# R2-Piler

A small compiler for the (invented) **R2-Lang** language, written in C.

R2-Piler takes a `.r2` source file through the whole pipeline — lexer, parser,
semantic analysis, IR, optimizer and x86-64 code generation — and writes AT&T
assembly. The result links with `gcc`; there is no runtime library to ship.

---

## Status

R2-Piler is a work in progress, but the implemented subset is usable and well
tested:

- **606 fixtures** pass (`make test`): 58 lexer, 104 parser, 200 sema, 75 IR,
  47 optimizer, 48 mangling/visibility/export, 14 `.r2i`, 16 C/C++ header and
  44 import cases.
- **182 executable assembly fixtures** pass (`make bench`) at `-O0`, `-O1`,
  `-O2` and `-O3`, including register-allocation and ABI stress tests.
- The only working backend is **x86-64** (System V AMD64 ABI). ARM, RISC-V and
  the interpreter are recognized on the command line but rejected as
  “backend is not implemented”.
- The language currently has integers, `enum`/`type`/`newtype` declarations,
  functions, locals, globals, blocks, `if`/`elif`/`else`, `while`, `for`,
  `switch`, `return`, casts, the ternary `?:`, short-circuit logic,
  the full integer/bitwise operator set, compound assignment (`+=`, `-=`, …)
  and postfix statement-only `++`/`--` (also in the `for` update), plus
  `static var`/`static arr` locals with static storage duration and
  constant initializers.
- **Pointer** support is complete end to end: `ptr T` types, `@` deref and
  `$` address-of, `NULL`, `sizeof`, C-like pointer arithmetic and comparison,
  explicit pointer casts and string literals (`TYPE_STR`, adopting `ptr i8`).
  Pointer arithmetic is lowered with C semantics: `p + n` scales by
  `sizeof(T)` and `p - q` yields the distance in **elements**. The backend
  lowers address-of, dereference, loads/stores and a deduplicated `.rodata`
  string pool; address-taken locals get a stack slot. Global pointer
  initializers must be link-time constants — `NULL`, a string literal, the
  address of a global (`$g`) or a function name for a `ptr fn` — and at
  `-O2`/`-O3` they are bound statically into `.data` (`.quad .LC0`,
  `.quad __r2_g`, `.quad <fn>`) instead of running in `__r2_init`.
- **Function pointers** are supported end to end: `ptr fn(a : i32) : i32`
  (`fn(...)` is an alias), signatures with pointers, arrays, slices, structs,
  `&T` parameters, multi-return (`(i32, u32)`) and C variadics (`...`),
  checked structurally on init/assignment/argument/return. Any fixed-signature
  `fn`/`extern fn`/`export fn` is a value (a bare name is already the pointer);
  functions with defaults or R2 typed variadics are not. They live in locals,
  parameters, returns, globals, `static` and struct fields, and are called with
  the ordinary postfix syntax (`cb(4, 5)`, `s.cb()`, `table[i](x)`,
  `(c ? f : g)(x)`) with positional, fixed-arity arguments. `==`/`!=` and
  `NULL` are allowed; arithmetic, ordering, `@` and `[]` have dedicated
  diagnostics; `ptr fn` converts implicitly to/from `ptr void`. The backend
  emits `leaq` (or `sym@GOTPCREL` for an `extern`), copies the callee to a
  scratch register and calls through it with `%al` zeroed; indirect calls are
  never tail calls.
- **`const`** is a type qualifier (`var x : const i64`, `ptr const T`,
  `const ptr T`, `const T[N]` for const elements). A const object needs an
  initializer and its lvalue is read-only, including members of a `const`
  struct and elements of a const array; reads drop only the outer qualifier
  (C lvalue-to-rvalue), implicit conversions may add const but never drop it
  (an explicit `as` is required), function signatures ignore the outer const
  of parameters/returns, and the `.r2i`/C header carry the qualifier.
- **C interop** through `extern fn` / `extern var` declarations: they have no
  body or initializer, calls are type-checked like normal functions, and the
  emitted symbol keeps its plain C name (`call puts`, `optind(%rip)`) with
  `gcc` resolving it at link time. `%al` is zeroed before every extern call
  (SysV variadic ABI). The reverse direction is `export fn` / `export var` /
  `export arr`: a plain `.globl` C symbol for R2 definitions (no mangling),
  restricted to C-representable signatures (no structs by value, slices,
  arrays/slices in returns or multi-return). A C host calls `__r2_init_<mod>`
  once per module that has runtime global initializers, but the symbol only
  exists when that function is non-empty (a module whose globals are all
  constant, the usual `-O2` case, emits none).
- Top-level symbols are file-private by default and mangled
  (`_r2_f_<mod>_<len><name>`, `.local` in assembly); `pub` makes them public
  (`_r2_pf_...` / `_r2_pv_...`, `.globl`) and `fn main` must be
  `pub fn main() : i64`. The module name is derived from the canonical source
  directory plus the file name as written (so a file symlink keeps its own
  name), relative to the deepest common ancestor of the current directory and
  the file (basename if only `/` is shared): `src/math.r2` → `3src4math`,
  with unsafe path bytes escaped as `$xx`. The generated-symbol prefixes
  `__r2_` and `_r2_` are reserved for top-level names.
- **Classes**: `class C { x : i64; init (self, ...) ...; fn m(self, ...) ...;
  priv fn p(self) ...; fn assoc(...) ... }`. Methods are ordinary functions
  whose first parameter is an implicit `self : ptr C`; `p.m(args)` inserts the
  receiver automatically (values are passed by address, pointers as-is,
  temporaries included), `C::m(args)` calls directly (an explicit receiver is
  required for methods), and bare `m(args)` inside a class resolves to a
  sibling method. `priv` members are visible only inside the class, fields and
  methods cannot share a name, and methods are emitted as local
  `_r2_m_<mod>_<class>_<method>` functions. `var p : C = C(args);` calls the
  class initializer, which is declared as `init (self, ...) { ... }` (no `fn`,
  no return type, at most one per class) and can only be invoked through
  `C(...)`, `new C(...)` or placement `new(slot) C(...)`. Construction is
  direct when the destination is fresh (locals, globals, `static`, returns,
  array elements and struct fields) and goes through a temporary elsewhere;
  `new C` without parentheses allocates without calling `init`. Class literals
  `{...}` are rejected. Classes still cannot be `pub`/`export`, and `op` and
  `destroy` are not implemented.
- Multidimensional arrays and slices of any rank are supported
  (`arr m : i32[R][C]`, `i32[][]` slice parameters), and structs/unions
  with C-like layout, designated `{.field = ...}` exhaustive initializers,
  field access `.` (auto-deref on `ptr struct`), by-value copies, by-value
  arguments and `sret` returns, arrays of structs, globals/`static` structs
  (constant initializers become a `.data` image at any level; pointer/slice/
  bitfield initializers are written from `__r2_init`) and `sizeof`.
- **Heap arrays**: `new T[N]` returns a `T[]` slice backed by `malloc`, `new T`
  returns a `ptr T`, and `new T[D1][D2]...` allocates a rank-N slice (one
  runtime length per dimension). A custom allocator can be supplied with
  `new(expr) T[N]`; `delete x;` frees a pointer or the data buffer of a slice,
  and `delete(expr) x;` calls a custom deallocator. The default forms need
  `import "stdlib";` (the built-in `malloc`/`free`), while the explicit
  allocator/deallocator forms do not. For classes, `new C(args)` allocates and
  calls `init`, while `new(slot) C(args)` initializes memory supplied by the
  caller (the slot must be `ptr C` or `ptr void`); bindings created with a
  custom allocator require `delete(deallocator) p;` rather than a plain
  `delete p;`.
- Not implemented yet: floating point, `defer`, class `op`/`destroy` and
  exported classes.

---

## Features

- A small, statically and strongly typed imperative language: eight integer
  types (`i8` to `u64`, with `char` as an alias of `i8`), explicit casts with
  `as`, and implicit widening only when no value can be lost.
- Full statement set: `var` declarations, blocks, `if`/`elif`/`else`, `while`,
  `for`, `switch` and `return`, plus top-level functions with parameters by
  value, recursion and forward references. `static var`/`static arr` locals
  keep one instance with static storage duration and a constant initializer.
- Pointers: `ptr T` and `ptr void` types, `@` dereference, `$` address-of,
  the `NULL` literal, `sizeof`, C-like pointer arithmetic and comparison, and
  explicit pointer-to-pointer casts.
- Function pointers: `ptr fn(a : i32, b : i32) : i32` (bare `fn(...)` is the
  same type) with structurally checked signatures, callable through any
  postfix expression, storable in globals, `static`, struct fields and arrays,
  and convertible implicitly to/from `ptr void`.
- Arrays and slices: `arr a : T[N]` with `{...}` and string initializers,
  element-scaled indexing, `.len`, `sizeof`, decay to `ptr T` (including in
  `a + i` / `a - i` / `a - b`), slice parameters `T[]` (pointer + hidden
  length) and fixed-length array parameters `T[N]` — passed as a bare pointer
  and length-checked at the call site, plus the index loop over them:
  `for (var i : i64 : a)`, `for (var i : a)` and `for (i : a)` — and one index
  per dimension (`for (var i : i64, j : i64 : mat)`), desugared by the parser
  into ordinary `for`s bounded by `a.len`, `a[i].len`, ...
- Heap allocation: `new T[N]` / `new T[D1][D2]...` (a `T[]`-style slice) and
  `new T` (a `ptr T`), `new(allocator_expr)` for custom allocators,
  `delete x;` and `delete(deallocator_expr) x;`. The default allocator/free
  require `import "stdlib";` and use `malloc`/`free`; memory is not
  zero-initialized. `new`/`delete` are also allowed in global and `static`
  initializers (they run from `__r2_init`, setting `has_dynamic_init`).
- String literals, typed as `string` and usable wherever `ptr i8` is expected.
- Top-level visibility: private by default, `pub` for public symbols (`.globl`,
  `_r2_pf_...`/`_r2_pv_...`) and `export` for plain C-linkable definitions;
  module names come from the canonical source path with `$xx`-escaped bytes.
- **Modules and imports**: one file is one module. `import "m";` exposes
  `m::name`, `import "m" as x;` renames the qualifier, and
  `from "m" import a as b, T;` brings names into scope. The importer reads
  `m.r2i` (source directory, then the `-m` directory, then each `-p DIR`),
  reconstructs the module's public types/signatures/defaults, treats its
  functions and globals as external symbols, and calls its `__r2_init_<mod>`
  when it has dynamic initializers. Only `pub`/`export`/`pub extern`
  declarations are visible; `from` does not bind the qualifier; local
  variables may shadow imported names but a top-level name collision is an
  error. Dependencies listed in the `.r2i` `imports:` header are loaded
  recursively (cycle-checked), so a module can re-export imported types.
- Top-level globals with well-defined initialization order; cyclic initializers
  are diagnosed.
- Short-circuit `&&` and `||`, and the ternary `cond ? a : b` (lowered to
  `select`/`cmov` when the condition and both arms are pure).
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

It also writes the module interface to `modules/<module>.r2i` (the module is
the source stem, or the name of a top-level `module "..."`; the directory is
flat). The file lists the `pub`/`export` interface in R2 declaration syntax
with already-mangled top-level symbols; its comment header carries the module
id, the modules whose types appear in that interface (`imports:`) and the
`has_dynamic_init` flag. Use `-m DIR`/`--modules-dir DIR` to change the
directory, `-n`/`--no-r2i` to skip it and `-p DIR`/`--path DIR` (repeatable) to
add directories searched by importers. The `.r2i` is written only after the
assembly succeeds, and an existing file generated from a different source is
never overwritten. Modules are compiled one at a time and linked afterwards:

```bash
./build/r2p a.r2 -m modules      # writes a.s and modules/a.r2i
./build/r2p b.r2 -m modules      # b.r2 has `import "a";`, reads modules/a.r2i
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
│   ├── parser.h     # Parser state and entry points
│   ├── common.h     # Private cross-file declarations for parser/
│   ├── parser.c     # Driver: token cursor, panic sync, node helpers, parse()
│   ├── expressions.c # Precedence, primaries, calls, casts, unary, ++/--
│   ├── types.c      # Type syntax: parse_type, sizeof, pointer types
│   ├── statements.c # Blocks, if/elif/else, while, for, return, var decls
│   ├── declarations.c # fn/extern decls, enums, type/newtype declarations
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

An error in a stage stops the compilation before the next stage; warnings do
not. The IR optimizer only runs at `-O2`/`-O3`; `-O1` keeps the IR unoptimized
but runs register allocation, and `-O0` spills every value to the stack.

---

## License

MIT

# R2-Piler

A small compiler for the (invented) **R2-Lang** language, written in C.

R2-Piler takes a `.r2` source file through the whole pipeline — lexer, parser,
semantic analysis, IR, optimizer and x86-64 code generation — and writes AT&T
assembly. The result links with `gcc`; there is no runtime library to ship.

---

## Status

R2-Piler is a work in progress, but the implemented subset is usable and well
tested: `make test` passes 633 fixtures and `make bench` passes 198 executable
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
  `C::m(...)` calls directly; `priv` hides members. An initializer (`init`) is
  invoked by `C(...)`, `new C(...)` or `new(slot) C(...)` (caller-provided
  memory), and an optional destructor by `drop`, `reset` and `delete`. Classes
  can overload operators (`op +`, `op []`, `op =`, compound and `op ++`/`--`)
  on class values, while `ptr C` keeps native pointer behaviour; class values
  also work as conditions and with a non-class left operand. `pub class` is
  visible to other R2 modules and `export class` is usable from C/C++.
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
- C interop: `extern fn` / `extern var` declarations refer to plain C symbols;
  `export` declarations produce C/C++ headers with `-H`/`--header`.
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
├── main.c, compiler.c   CLI and compilation driver
├── type.c               dynamic type table
├── arena/               bump allocator; owns all compiler memory
├── error/               file:line:col diagnostics
├── lexer/               tokens
├── parser/              AST and syntax
├── sema/                symbols, type checking, warnings
├── imports/             .r2i reader and import lookup
├── r2i/                 .r2i writer
├── header/              C/C++ header writer
├── mangle/              module names and symbol mangling
├── ir/                  IR generation, CFG/liveness and optimizer (opts/)
├── codegen/             backend framework, register allocation (regalloc/)
│                        and the x86-64 emitter (x86-64/)
└── util/                small shared helpers
```

Source is auto-discovered (`find src -name '*.c'` in the Makefile); there is no
list of source files to maintain.

### Pipeline

```
source -> lexer -> parser -> imports -> sema -> IR -> optimizer -> x86-64 asm
```

An error in a stage stops compilation; warnings do not. The IR optimizer runs
at `-O2`/`-O3`; `-O1` allocates registers without optimizing, and `-O0` spills
every value to the stack.

---

## License

MIT

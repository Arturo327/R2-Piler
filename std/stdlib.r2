// std/stdlib.r2 — utilidades generales (stdlib.h).
//
// Gestión de memoria dinámica, conversión de texto a número y ordenación.
// `qsort` recibe un `ptr fn` con la firma del comparador de C;
// `size_t` se declara como `u64`.
//
// Uso:
//   from stdlib import malloc, free, atoi;
//   import stdlib as c;        c::exit(0);

module "stdlib";

// memoria dinámica
pub extern fn malloc (n : u64) : ptr void;
pub extern fn calloc (n : u64, size : u64) : ptr void;
pub extern fn realloc (p : ptr void, n : u64) : ptr void;
pub extern fn free (p : ptr void) : void;

// conversión de texto
pub extern fn atoi (s : ptr i8) : i32;
pub extern fn atol (s : ptr i8) : i64;
pub extern fn getenv (name : ptr i8) : ptr i8;

// conversión con puntero al primer carácter no consumido (char **)
pub extern fn strtol (s : ptr i8, end : ptr ptr i8, base : i32) : i64;
pub extern fn strtoul (s : ptr i8, end : ptr ptr i8, base : i32) : u64;
pub extern fn strtoll (s : ptr i8, end : ptr ptr i8, base : i32) : i64;
pub extern fn strtoull (s : ptr i8, end : ptr ptr i8, base : i32) : u64;

// ordenación y búsqueda: int (*)(const void *, const void *)
pub extern fn qsort (base : ptr void, n : u64, size : u64,
		cmp : ptr fn(a : ptr void, b : ptr void) : i32) : void;
pub extern fn bsearch (key : ptr void, base : ptr void, n : u64, size : u64,
		cmp : ptr fn(a : ptr void, b : ptr void) : i32) : ptr void;

// ejecutar un comando de la shell
pub extern fn system (cmd : ptr i8) : i32;

// terminación del proceso
pub extern fn exit (code : i32) : void;
pub extern fn abort () : void;

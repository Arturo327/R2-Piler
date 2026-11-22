// std/math.r2 — operaciones matemáticas enteras sobre libc.
//
// Todas las declaraciones son `pub extern`, por lo que los símbolos se
// exportan tal cual (abs, labs, rand, ...) y el enlazador los resuelve
// contra libc. No se genera código propio: este módulo solo es una interfaz.
//
// Uso:
//   import math;              math::abs(-5)
//   from math import abs;     abs(-5)

module "math";

// valor absoluto (int / long / long long en C; en x86-64 Linux son 32/64/64)
pub extern fn abs (x : i32) : i32;
pub extern fn labs (x : i64) : i64;
pub extern fn llabs (x : i64) : i64;

// generador pseudoaleatorio
pub extern fn rand () : i32;
pub extern fn srand (seed : u32) : void;

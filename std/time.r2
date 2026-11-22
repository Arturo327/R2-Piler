// std/time.r2 — reloj de libc (time.h).
//
// `time` devuelve `time_t` (i64 en x86-64) y admite un puntero opcional donde
// almacenar el resultado; se pasa NULL para ignorarlo.
//
// Uso:
//   import "time" as tm;       var t : i64 = tm::time(NULL);
//   from "time" import clock;  clock();

module "time";

pub extern fn time (out : ptr i64) : i64;
pub extern fn clock () : i64;

// Las funciones que toman structs de <time.h> reciben el puntero como
// `ptr void` (handle opaco). Se les puede pasar `$mi_struct` siempre que la
// struct local tenga el layout de C; así el módulo no fija su representación.
pub extern fn strftime (dst : ptr i8, n : u64, fmt : ptr i8, tm : ptr void) : u64;
pub extern fn mktime (tm : ptr void) : i64;
pub extern fn gettimeofday (tv : ptr void, tz : ptr void) : i32;
pub extern fn nanosleep (req : ptr void, rem : ptr void) : i32;

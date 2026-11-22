// std/unistd.r2 — E/S de bajo nivel y suspensión (unistd.h).
//
// `ssize_t` es `i64` en x86-64 Linux (LP64). `open` es variádico en C: el
// tercer argumento (mode) solo se pasa cuando flags incluye O_CREAT.
//
// Uso:
//   from "unistd" import read, write, close;
//   write(1, "hola\n" as ptr void, 5);

module "unistd";

// descriptores
pub extern fn read (fd : i32, buf : ptr void, n : u64) : i64;
pub extern fn write (fd : i32, buf : ptr void, n : u64) : i64;
pub extern fn open (path : ptr i8, flags : i32, ...) : i32;
pub extern fn close (fd : i32) : i32;

// suspensión (segundos, devuelve los segundos que quedaron)
pub extern fn sleep (seconds : u32) : u32;

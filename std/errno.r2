// std/errno.r2 — acceso al errno de libc (errno.h).
//
// En glibc `errno` no es un símbolo de variable: es un macro que llama a
// __errno_location(). Por eso se declara la función y se lee/escribe el i32
// apuntado con `@`:
//
//   from "errno" import __errno_location;
//   @__errno_location() = 0;              // limpiar errno
//   var e : i32 = @__errno_location();    // consultar errno
//
// Es específico de glibc (no existe en musl ni en macOS).

module "errno";

pub extern fn __errno_location () : ptr i32;

// std/string.r2 — cadenas y memoria (string.h).
//
// Las funciones de memoria usan `ptr void` (equivalente a void* de C); las de
// texto usan `ptr i8` (equivalente a char*). `size_t` es `u64` en x86-64.
//
// Uso:
//   from "string" import strlen as slen, strcmp;
//   import "string" as str;    str::memset(buf, 0, 16);

module "string";

// longitud y comparación
pub extern fn strlen (s : ptr i8) : u64;
pub extern fn strcmp (a : ptr i8, b : ptr i8) : i32;
pub extern fn strncmp (a : ptr i8, b : ptr i8, n : u64) : i32;

// copia y concatenación
pub extern fn strcpy (dst : ptr i8, src : ptr i8) : ptr i8;
pub extern fn strncpy (dst : ptr i8, src : ptr i8, n : u64) : ptr i8;
pub extern fn strcat (dst : ptr i8, src : ptr i8) : ptr i8;
pub extern fn strncat (dst : ptr i8, src : ptr i8, n : u64) : ptr i8;

// búsqueda
pub extern fn strchr (s : ptr i8, c : i32) : ptr i8;
pub extern fn strrchr (s : ptr i8, c : i32) : ptr i8;
pub extern fn strstr (s : ptr i8, sub : ptr i8) : ptr i8;
pub extern fn memchr (s : ptr void, c : i32, n : u64) : ptr void;

// dividir en tokens y describir errores
pub extern fn strtok (s : ptr i8, delim : ptr i8) : ptr i8;
pub extern fn strerror (errnum : i32) : ptr i8;

// memoria en bruto
pub extern fn memcpy (dst : ptr void, src : ptr void, n : u64) : ptr void;
pub extern fn memmove (dst : ptr void, src : ptr void, n : u64) : ptr void;
pub extern fn memset (dst : ptr void, c : i32, n : u64) : ptr void;
pub extern fn memcmp (a : ptr void, b : ptr void, n : u64) : i32;

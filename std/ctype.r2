// std/ctype.r2 — clasificación y conversión de caracteres (ctype.h).
//
// En C estas funciones toman un `int` (el valor de un `unsigned char` o EOF),
// por eso el parámetro es `i32`.
//
// Uso:
//   from ctype import toupper, isdigit;
//   if (isdigit('7') != 0) { ... }

module "ctype";

pub extern fn isalpha (c : i32) : i32;
pub extern fn isdigit (c : i32) : i32;
pub extern fn isalnum (c : i32) : i32;
pub extern fn isupper (c : i32) : i32;
pub extern fn islower (c : i32) : i32;
pub extern fn isspace (c : i32) : i32;
pub extern fn isblank (c : i32) : i32;
pub extern fn iscntrl (c : i32) : i32;
pub extern fn isgraph (c : i32) : i32;
pub extern fn isprint (c : i32) : i32;
pub extern fn ispunct (c : i32) : i32;
pub extern fn isxdigit (c : i32) : i32;
pub extern fn toupper (c : i32) : i32;
pub extern fn tolower (c : i32) : i32;

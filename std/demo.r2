// std/demo.r2 — demostración de importación de la librería estándar.
//
// Ejercita las cuatro formas de importar:
//   import math;                       -> math::abs(...)
//   import io as out;                  -> out::puts(...)
//   from string import strlen as slen  -> slen(...)
//   from ctype import toupper          -> toupper(...)
//
// Los `.r2i` están en este mismo directorio, así que no hace falta
// pasar -p. Compilar con `make -C std demo`.

module "demo";

// --- import cualificado ---
import math;
import time as tm;

// --- import con alias ---
import io as out;
import string as mem;

// --- from ... import, con y sin alias ---
from string import strlen as slen, strcmp as cmp;
from stdlib import malloc, free, atoi;
from ctype import toupper, isdigit;
from io import FILE;                       // tipo importado desde otro módulo

pub fn main () : i64 {
    // math
    if (math::abs(-7) != 7) return 1;
    if (math::labs(-1234567890123) != 1234567890123) return 2;
    math::srand(1);
    var r1 : i32 = math::rand();
    math::srand(1);
    if (math::rand() != r1) return 3;

    // io: printf variádico a través de un alias
    out::puts("== demo de la libreria estandar ==");
    out::printf("abs(-5) = %d\n", math::abs(-5));

    // FILE: handle opaco importado como tipo
    var f : FILE = out::fopen("/dev/null", "w");
    if (f == NULL) return 16;
    if (out::fputs("escribiendo en /dev/null\n", f) < 0) return 17;
    if (out::fflush(f) != 0) return 18;
    if (out::fclose(f) != 0) return 19;

    // io + string: sprintf sobre un buffer local
    arr buf : i8[64];
    out::sprintf($buf[0], "n=%d", 42);
    if (slen($buf[0]) != 4) return 4;
    if (cmp($buf[0], "n=42") != 0) return 5;
    if (cmp("abc", "abd") >= 0) return 6;

    // string: memset / memcpy con qualified alias
    arr membuf : i8[8];
    mem::memset($membuf[0], 65, 8);       // 'A'
    if (membuf[0] != 65 || membuf[7] != 65) return 7;
    mem::memcpy($membuf[0], $buf[0], 5);       // incluye el NUL final
    if (cmp($membuf[0], "n=42") != 0) return 8;

    // stdlib: memoria dinámica y conversión
    var p : ptr void = malloc(8);
    if (p == NULL) return 9;
    var q : ptr i64 = p as ptr i64;
    @q = 99;
    if (@q != 99) return 10;
    free(p);
    if (atoi("123") != 123) return 11;

    // ctype
    if (toupper('a') != 'A') return 12;
    if (isdigit('7') == 0 || isdigit('x') != 0) return 13;

    // time
    var t : i64 = tm::time(NULL);
    if (t <= 0) return 14;
    if (tm::clock() < 0) return 15;

    out::puts("todo OK");
    return 0;
}

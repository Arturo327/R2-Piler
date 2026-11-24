// std/examples/libc_extras.r2 — ejemplos de la cobertura ampliada de libc.
//
// Ejercita los cuatro casos del análisis:
//   - qsort con un comparador `ptr fn`
//   - printf variádico con varios argumentos
//   - fwrite/fseek/fread sobre un fichero
//   - for indexado sobre un slice
//
// Además comprueba strtol/strtoul, strrchr/memchr, isxdigit/isblank,
// write (unistd) y __errno_location (errno). Se compila desde std/ con
// `make -C std libc_extras` (el .r2i está en std/, se indica con -p .).

module "libc_extras";

import "io" as out;
import "string" as mem;
import "stdlib" as c;
from "io" import FILE;
from "io" import fread, fwrite, fseek;
from "stdlib" import qsort, bsearch, strtol, strtoul;
from "ctype" import isxdigit, isblank;
from "unistd" import write;
from "errno" import __errno_location;

fn cmp_i64 (a : ptr void, b : ptr void) : i32
{
    var x : ptr i64 = a as ptr i64;
    var y : ptr i64 = b as ptr i64;

    if (@x < @y) return -1;
    if (@x > @y) return 1;
    return 0;
}

pub fn main () : i64
{
    // 1. qsort con ptr fn y bsearch con el mismo comparador
    arr vals : i64[5] = { 5, 1, 4, 2, 3 };
    c::qsort($vals[0], 5, sizeof(i64), cmp_i64);
    if (vals[0] != 1 || vals[1] != 2 || vals[2] != 3
            || vals[3] != 4 || vals[4] != 5) return 1;

    var key : i64 = 3;
    var found : ptr void = bsearch($key, $vals[0], 5, sizeof(i64), cmp_i64);
    if (found == NULL) return 2;
    if (@(found as ptr i64) != 3) return 3;

    // 2. printf variádico con varios argumentos
    out::printf("qsort: %ld %ld %s %c\n", vals[0], vals[4], "ok", 'X');

    // 3. fwrite/fseek/fread sobre un fichero temporal en build/
    var f : FILE = out::fopen("build/libc_extras.bin", "w+");
    if (f == NULL) return 4;
    arr bytes : i8[8] = { 'r', '2', '\n', 0, 0, 0, 0, 0 };
    if (fwrite($bytes[0], 1, 3, f) != 3) return 5;
    if (out::fflush(f) != 0) return 6;
    if (fseek(f, 0, 0) != 0) return 7;
    arr back : i8[8];
    if (fread($back[0], 1, 3, f) != 3) return 8;
    if (back[0] != 'r' || back[1] != '2' || back[2] != '\n') return 9;
    if (out::fclose(f) != 0) return 10;

    // 4. for indexado sobre un slice
    var view : i64[] = vals;
    var total : i64 = 0;
    for (var i : view) {
        total += view[i];
    }
    if (total != 15) return 11;

    // cobertura adicional de las nuevas envoltorias
    var end : ptr i8 = NULL;
    if (strtol("42x", $end, 10) != 42) return 12;
    if (end == NULL || @end != 'x') return 13;
    if (strtoul("ff", NULL, 16) != 255) return 14;
    if (mem::strrchr("a/b/c", '/') == NULL) return 15;
    if (mem::memchr($bytes[0], '2', 3) == NULL) return 16;
    if (isxdigit('F') == 0 || isblank(' ') == 0) return 17;

    // unistd + errno
    if (write(1, "libc_extras OK\n" as ptr void, 15) != 15) return 18;
    @__errno_location() = 0;

    return 0;
}

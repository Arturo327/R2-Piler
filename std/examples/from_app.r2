// std/examples/from_app.r2 — importar la librería estándar desde otro directorio.
//
// Los .r2i viven en std/, así que se indica esa carpeta con -p:
//   ./build/r2p std/examples/from_app.r2 -p std -n -o /tmp/from_app.s
//   gcc /tmp/from_app.s -o /tmp/from_app && /tmp/from_app
//
// Demuestra además que `from ... import ... as ...` funciona igual aunque el
// módulo importado no esté junto al fichero que importa.

module "from_app";

import "math";
import "io" as out;
from "ctype" import isdigit, toupper as upper;
from "string" import strlen as len;

pub fn main () : i64 {
    if (math::abs(-11) != 11) return 1;
    if (isdigit('5') == 0 || upper('z') != 'Z') return 2;
    if (len("abcd") != 4) return 3;
    out::printf("from_app OK: %d %c %lu\n", math::abs(-11), upper('z'), len("abcd"));
    return 0;
}

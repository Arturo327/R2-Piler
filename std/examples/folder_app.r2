// std/examples/folder_app.r2 — importar la carpeta std/ entera.
//
// `import "std";` carga todos los .r2i de std/ y cada módulo queda bajo su ruta:
//   std::math::abs, std::time::time, std::io::puts, ...
// Desde examples/ la carpeta no está junto al fuente, así que se añade el
// padre con -p .. (el Makefile de std/ ya lo hace).

module "folder_app";

import "std";

pub fn main () : i64
{
    if (std::math::abs(-5) != 5) return 1;
    if (std::time::time(NULL) <= 0) return 2;
    std::io::puts("import std; OK");
    return 0;
}

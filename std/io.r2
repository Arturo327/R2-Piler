// std/io.r2 — entrada/salida de libc (stdio.h).
//
// Los `pub extern fn` variádicos (`...`) mantienen los argumentos extra sin
// comprobar, igual que en C. `stdin`/`stdout`/`stderr` son variables externas
// de tipo FILE.
//
// Uso:
//   import "io" as out;               out::printf("x=%d\n", x);
//   from "io" import printf, puts;    puts("hola");
//   from "io" import FILE, fopen;     var f : FILE = fopen("a.txt", "r");

module "io";

// FILE* de C es un handle opaco: `newtype` sobre `ptr void` lo mantiene
// nominalmente distinto de cualquier otro puntero, sin poder mezclarlo con
// ellos sin un `as` explícito. Se nombra como `io::FILE` o `from "io" import FILE`.
pub newtype FILE = ptr void;

// flujos estándar
pub extern var stdin : FILE;
pub extern var stdout : FILE;
pub extern var stderr : FILE;

// apertura y cierre de flujos
pub extern fn fopen (path : ptr i8, mode : ptr i8) : FILE;
pub extern fn fclose (stream : FILE) : i32;

// salida formateada
pub extern fn printf (fmt : ptr i8, ...) : i32;
pub extern fn fprintf (stream : FILE, fmt : ptr i8, ...) : i32;
pub extern fn sprintf (dst : ptr i8, fmt : ptr i8, ...) : i32;
pub extern fn snprintf (dst : ptr i8, n : u64, fmt : ptr i8, ...) : i32;

// entrada formateada (los extra son sin comprobar, como en C)
pub extern fn scanf (fmt : ptr i8, ...) : i32;
pub extern fn fscanf (stream : FILE, fmt : ptr i8, ...) : i32;
pub extern fn sscanf (s : ptr i8, fmt : ptr i8, ...) : i32;

// salida sin formato
pub extern fn puts (s : ptr i8) : i32;
pub extern fn putchar (c : i32) : i32;
pub extern fn fputs (s : ptr i8, stream : FILE) : i32;
pub extern fn fflush (stream : FILE) : i32;

// entrada
pub extern fn getchar () : i32;
pub extern fn fgets (dst : ptr i8, n : i32, stream : FILE) : ptr i8;

// lectura y escritura en bloques (size_t -> u64)
pub extern fn fread (dst : ptr void, size : u64, n : u64, stream : FILE) : u64;
pub extern fn fwrite (src : ptr void, size : u64, n : u64, stream : FILE) : u64;

// posicionamiento y estado del flujo (long -> i64)
pub extern fn fseek (stream : FILE, offset : i64, whence : i32) : i32;
pub extern fn ftell (stream : FILE) : i64;
pub extern fn rewind (stream : FILE) : void;
pub extern fn feof (stream : FILE) : i32;

// diagnósticos
pub extern fn perror (s : ptr i8) : void;

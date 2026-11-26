# R2-Lang — Especificación del lenguaje

Especificación completa de R2-Lang, el lenguaje que compila R2-Piler.

---

## 1. Panorama

R2-Lang es un lenguaje imperativo, estática y fuertemente tipado. Las conversiones
explícitas se escriben con `expr as type`; además existen ensanchamientos
implícitos silenciosos cuando el valor no puede perderse (mismo signo a más
ancho, o sin signo a un tipo con signo mayor). Cualquier otra conversión exige
un cast explícito.

- Tipos primitivos: `i8`, `u8`, `i16`, `u16`, `i32`, `u32`, `i64`, `u64` y `void`. `char` es un alias de `i8` aceptado solo en el frontend (lexer/parser): en dumps, símbolos y mensajes siempre aparece `i8`.
- Enums nominales definidos por el usuario (`enum Dias : u8 { Lunes, Martes }`), con un tipo subyacente entero (por defecto `u32`). Ver §3.1.
- Aliases de tipo transparentes (`type Index = u32`) y tipos nominales nuevos (`newtype Seconds = u32`). Ver §3.2.
- Punteros (`ptr T`, `ptr void`, `ptr fn(...)`), `$x` dirección, `@p` desreferencia, `p[i]` indexación, el literal `NULL`, `sizeof`/`alignof`/`offsetof`, aritmética y comparación de punteros y casts solo entre punteros. Ver §3 y §5.
- Literales string, de tipo `string`, que se comportan como `ptr i8` donde se espera uno. Ver §2.5.
- Arrays concretos de rango N (`arr m : i32[2][3]`) con inicializadores exhaustivos/designados, `.len`/`sizeof`/`alignof`, indexación y decay a puntero; vistas locales **no propietarias** (`var v : i32[2][3] = m`); slices `T[]`/`T[][]`/... y tipos mixtos `T[R][]` como parámetros, locales y tipos de retorno (un array concreto se devuelve como puntero). Ver §3.3 y §8.
- Structs y uniones (`struct P { x : i32; y : i32; }`, `union V { i : i64; b : u8[8]; }`) con layout C (y `packed struct` sin padding), miembros anidados anónimos, **bitfields** (`x : u8 : 3`, `x : u3`), inicializadores designados parciales con zero-fill y literales `{...} as T`, acceso `.` con auto-desreferencia sobre punteros, copia por valor, paso de argumentos y retorno por valor. Ver §3.4.
- El tipo builtin `any` es un `struct` de 16 bytes (`tag : ptr i8`, `value : i64`) que empaqueta en silencio cualquier escalar, enum, newtype, puntero, string o `NULL`; `.tag` da el nombre del tipo original y `x as T` lo desembala. Ver §3.4 y §8.
- Statements: `if`/`elif`/`else`, `while`/`for` con `else` opcional (salida natural), `for` (C-style y por índice sobre arrays/slices), `break`/`continue` (también con nivel: `break N`, `continue N`), `switch` con `case`/`default`, `static var`/`static arr` dentro de funciones, la multi-asignación `a, b = f();` y el ternario `?:` como expresión. Ver §5 y §6.
- Funciones solo a nivel top-level, con parámetros por valor o por referencia (`&T`), un parámetro variádico final opcional (`fn f (fmt : ptr i8, rest : ...i64)`, que dentro de la función es un slice `i64[]`, más `...any` para valores heterogéneos), *spread* de un slice en una llamada variádica (`f(s...)`) y uno o varios valores de retorno (`fn f() : i64, u32`). Ver §8.
- **Punteros a función** (`ptr fn(a : i32, b : i32) : i32`; `fn(...)` sin `ptr` es lo mismo): el nombre de cualquier función de firma fija es un valor de ese tipo, se guardan en locales/parámetros/retornos/globales/`static`/campos de struct y se llaman con la sintaxis postfija normal (`cb(4, 5)`, `s.cb()`, `table[i](x)`), con la firma comprobada al asignar. Ver §8.2.
- Interoperabilidad con C vía declaraciones `extern fn` / `extern var`, que no llevan cuerpo ni init y se emiten con su nombre C sin mangling; una `extern fn` puede terminar en `, ...` sin tipo (`printf`). Ver §8.1.
- Visibilidad top-level: privado por defecto (`pub` para exportar a nivel R2) y `export` para exponer un símbolo plano enlazable desde C, también en structs/uniones/enums/`type`/`newtype`. Un fichero puede renombrar su módulo con `module "nombre";`, que sustituye al nombre derivado de la ruta. Ver §9.1.
- Scopes de bloque con shadowing entre scopes.
- Entry point: `pub fn main() : i64`.
- El compilador emite assembly x86-64 AT&T (no binarios: el binario final se produce ensamblando, p. ej. `gcc prog.s -o prog`). Una compilación sin flags de dump escribe `<fuente>.s` (la ruta del fuente con la extensión cambiada por `.s`) o la ruta de `-o`. Con `-H`/`--header` escribe además una cabecera C/C++ con lo declarado `export`.

Pipeline del compilador:

```
lexer -> parser -> sema -> IR -> optimize_ir -> codegen (assembly)
```

Un error en cualquier fase aborta la compilación antes de la siguiente. Los warnings no abortan.

---

## 2. Estructura léxica

### 2.1 Comentarios

`//` hasta el fin de línea y `/* ... */` para bloques (no anidados: cierran en el primer `*/`). Solo `\n` termina un comentario de línea: un `\r` solitario no es fin de línea y lo consume como un carácter más. Un `/*` sin cerrar es error (`block comment is not closed`).

### 2.2 Identificadores y keywords

Identificadores: `[a-zA-Z_][a-zA-Z0-9_]*`.

Keywords (una palabra clave nunca puede usarse como identificador):

```
fn  if    for    arr   i8   u8   i16  u16  i32  u32
i64  u64   var   char  elif  else  void
while  return  as  enum  type  newtype  extern
export  pub
break  continue  switch  case  default  static
ptr  NULL  sizeof  struct  union  packed
alignof  offsetof  module  import  from  any  depends
new  delete  drop  reset  class  priv
```

`ptr`, `NULL`, `sizeof`, `packed`, `alignof`, `offsetof`, `pub`, `export`, `module`, `import`, `from`, `any` y `depends` son keywords (no identificadores). `::` es un único token (el cualificador de módulo) y no puede aparecer con espacios entre los dos `:`. `@` y `$` son operadores unarios, no palabras clave. En un bitfield, `uN`/`iN` (`u3`, `i5`, ...) **no** son keywords: se lexan como identificadores y solo el parser de campos de struct les da significado (§3.4).

### 2.3 Literales enteros

- Decimal: `42`. Hexadecimal: `0x10` / `0X10`. Octal: `0o17` / `0O17`. Binario: `0b101` / `0B101`.
- Los decimales con ceros a la izquierda (`007`) son decimales, no octales. `0` solo es decimal.
- Sufijo `u` / `U` → literal `u64`; sin sufijo → literal `i64` en crudo (el valor se guarda sin interpretar el signo; es sema quien lo tipa según el contexto).
- Desbordar los 64 bits en cualquier base no es error: el lexer emite un **warning** y trunca el valor a 64 bits (`literal does not fit in 64 bits, truncated to …`). Dígito inválido para la base o prefijo sin dígitos: error.
- `2^63` sin sufijo (`9223372036854775808`, `0x8000000000000000`, `0o1000000000000000000000`, `0b1` + 63 ceros) se lexa como `TOK_LIT_i64` con el valor en crudo; el parser lo deja tal cual (incluso tras `-`: `-9223372036854775808` es `NEG(lit)` en el AST) y es **sema** quien lo resuelve: `-2^63` se pliega a un único literal `INT64_MIN` válido, un `2^63` positivo suelto es error (`literal … does not fit in type i64; it is too large (the 'u' suffix makes it a u64 literal)`), y en contexto `u64` el literal adopta `u64` en silencio.
- Se admite `_` como separador entre dígitos (`1_000`, `0xFF_FF`, `0b1010_1010`); no puede ir al inicio, al final ni justo tras el prefijo (`_42`, `42_`, `0x_FF` son error).

### 2.4 Literales char

- `'c'`, exactamente un carácter. Vacío, multichar, newline dentro o sin cerrar: error.
- Escapes válidos: `\n` `\t` `\r` `\0` `\\` `\'` `\"` y `\xHH` (exactamente dos dígitos hexadecimales; `'\x41'` es `'A'`). Escapes desconocidos: error.
- Un literal char tiene tipo `i8` (1 byte con signo, rango -128..127). La keyword `char` puede usarse en lugar de `i8` en declaraciones de tipos, parámetros, retornos y casts: es un alias puro del frontend.

### 2.5 Literales string

- `"..."` con los mismos escapes que char, más `\<newline>` como continuación de línea.
- Un string no puede contener un newline real sin escapar.
- Un literal string tiene tipo `string` (`TYPE_STR`) y **adopta `ptr i8`** en cualquier contexto que lo pida: `var p : ptr i8 = "abc";`, `f("abc")` con `f(p : ptr i8)`, `return "abc";` desde una función `ptr i8`. `sizeof("abc")` vale `4` (los bytes más el terminador nulo) y el literal vive en memoria estática de solo lectura.
- Un literal string **no** es un valor de verdad: `if ("abc")`, `!"abc"`, `"abc" && x` son error. Un puntero (`ptr T`) y `NULL` sí lo son.
- En aritmética y comparaciones se comporta como `ptr i8` (es un puntero más): `"abc" + 1` es `ptr i8` y `p == "abc"` vale con `p : ptr i8`. Restar dos literales string (`"abc" - "ab"`) es válido — da la distancia en elementos (aquí bytes, porque un literal string es `ptr i8`) — pero advierte (`subtracting two string literals is probably a mistake`). No hay concatenación de strings.

---

## 3. Tipos

| Tipo | Tamaño | Rango / representación |
|------|--------|------------------------|
| `i8` (`char`) | 1 byte | Con signo, -128..127. |
| `u8` | 1 byte | Sin signo, 0..255. |
| `i16` | 2 bytes | Con signo, -32768..32767. |
| `u16` | 2 bytes | Sin signo, 0..65535. |
| `i32` | 4 bytes | Con signo. |
| `u32` | 4 bytes | Sin signo. |
| `i64` | 8 bytes | Entero con signo, complemento a 2. Aritmética con wrap definido. |
| `u64` | 8 bytes | Entero sin signo, módulo 2^64. |
| `enum E : B` | = `B` | Tipo nominal entero; se representa como su tipo subyacente `B` (por defecto `u32`). |
| `type X = T` | = `T` | Alias transparente: `X` **es** `T` (se canonicaliza y no llega a sema/IR/codegen). |
| `newtype X = T` | = `T` | Tipo nominal distinto con la misma representación física que `T`. |
| `struct X` / `union X` | calculado | Tipo nominal agregado: campos con offsets C-like y alineación natural; la unión solapa todos sus miembros en el offset 0. |
| `any` | 16 bytes | Struct builtin `{ tag : ptr i8; value : i64; }` (align 8) para valores heterogéneos: empaqueta en silencio escalares, enums, newtypes, punteros, strings y `NULL` (§3.4). |
| `ptr T` | 8 bytes | Puntero a `T`. |
| `ptr void` | 8 bytes | Puntero sin tipo apuntado. Convierte **en silencio** desde y hacia cualquier `ptr T` (init, asignación, argumento, `return`); sin aritmética ni desreferencia. |
| `ptr fn(params) : R` | 8 bytes | Puntero a una firma de función (§8.2). `fn(...)` sin `ptr` es el mismo tipo, así que `var cb : fn(a : i32) : i32` y `var cb : ptr fn(a : i32) : i32` son idénticos. Una firma **suelta** (sin `ptr`) nunca es un valor. |
| `T[N]` | `N * sizeof(T)` | Array concreto de `N` elementos de tipo `T`, contiguo (row-major). Se declara con `arr` o con `var`; `N` es una constante entera > 0. |
| `T[R][C]…` | `R*C*…*sizeof(leaf)` | Array concreto multidimensional row-major (la primera dimensión es la más externa); se indexa `m[i][j]…`. Con `arr` o `var`. |
| `T[]` | 16 bytes (ABI) | Slice/vista de rango 1: puntero a los datos + longitud. Parámetros, locales y tipo de retorno. |
| `T[][]` ... `T[]...[]` | `8 * (1 + N)` bytes (ABI) | Slice genérico de rango N: puntero al elemento hoja + una longitud `i64` por dimensión (rango 2 = 24 bytes, rango 3 = 32, ...). Parámetros, locales y tipo de retorno. |
| `T[2][]`, `T[][3]`, ... | descriptor | Tipo mixto: dimensiones escritas (estáticas, en el tipo) y `[]` (longitudes runtime). `sizeof` es el del descriptor: `8 * (1 + dinámicas)` (§3.3). |
| `nil` (`NULL`) | 8 bytes | Tipo propio del único valor `NULL`. Se convierte en silencio a cualquier `ptr T`. |
| `string` (`"..."`) | 8 bytes | Tipo propio del literal string. Se convierte en silencio a `ptr i8`. |
| `void` | — | Solo como tipo de retorno de función. |

`char` es un alias de `i8` aceptado solo donde se escribe un tipo (`var x : char`, `fn f(a : char) : char`, `x as char`). No existe como tipo propio: en `--dump-ast`, `--dump-symbols`, `--dump-ir` y en todos los mensajes aparece `i8`.

Reglas de tipos (estrictas; los únicos implícitos son el ensanche que no puede perder valor, `NULL`, el literal string y `ptr void`):

- **Literales flexibles**: un literal (o `-literal`, o una expresión hecha solo de literales) adopta en silencio el tipo que le pide el contexto si su valor cabe en él: `var u : u64 = 1`, `take_i64('a')`, `c > 5` (el `5` se vuelve `i8`), `var v : u64 = 9223372036854775808` (sin sufijo, cabe en `u64`). Las subexpresiones puramente literales se **pliegan en compilación** con semántica de wrap, y el chequeo de rango se hace sobre el valor plegado: `var w : char = 100 * 100` es error (`literal 10000 does not fit in type i8`, reportado en el operador). Los casts de literales también pliegan (el valor se trunca al tipo del cast antes de plegar): `-(5 as u64)` es la constante `u64` exacta, `(300 as u8) > 200` pliega a `0`. El divisor cero conocido (literal, plegado o tras un cast) es **error de compilación** (`division by zero`, reportado en el divisor; `division by zero in constant expression`, reportado en el operador, si ambos operandos son literales). No se pliegan y se dejan al hardware: `div`/`mod` con signo por `-1` sobre el mínimo del tipo (`INT64_MIN / -1`, `(-128 as i8) / (-1 as i8)`) y los shifts con cuenta `>= 64`.
- **Ancho del plegado**: toda subexpresión hecha solo de literales (untyped) pliega con wrap a **64 bits**, sea cual sea el sabor del literal: `var x : i64 = 'a' + 'b';` da **195**. Solo los pares de operandos con tipo concreto narrow (p. ej. dos casts) pliegan con truncado al ancho de ese tipo (`(250 as u8) + 10` pliega a `4`, lo mismo que haría `add.u8` en runtime). Consecuencia: una constante que no cabe en el tipo destino es error aunque "envolvería" bien en un tipo más ancho (`var a : i8 = 1 << 7;` → `literal 128 does not fit in type i8`).
- **Locación de los errores de rango**: un literal suelto se reporta **en el literal** (`c + 300` marca el `300`); un `-literal` marca el `-literal` **completo**; una subexpresión plegada se reporta **en el operador** (el nodo plegado hereda la posición del operador), y un init en el nombre declarado.
- **Shifts con izquierda untyped**: si la cuenta es un literal, la constante plegada queda flexible y adopta el tipo del contexto (`var v : u64 = 1 << 2;` vale). Si la cuenta **no** es un literal, la izquierda finaliza a su tipo por defecto (`1 << x` es `i64`, `'a' << x` es `i8`) y el shift ya no adopta el contexto: `var a : i8 = 1 << x;` es un error de estrechamiento, igual que `var a : i8 = 1 << 7;`. Migración: `var v : u64 = 1 << x;` → `var v : u64 = (1 << x) as u64;` (la misma regla que ya aplicaba a `var v : u64 = 5 + x;`).
- **Literal fuera de rango**: error (`literal 300 does not fit in type i8; use an explicit cast with 'as' to reinterpret the bits`). Si además el destino es con signo y el valor supera `INT64_MAX`, el mensaje añade que el sufijo `u` lo convierte en literal `u64`. Un literal negativo hacia un destino sin signo es error, también dentro de binarios (`var u : u64 = -5;`, `5u + -1`); los mensajes siempre usan el nombre público del tipo (`u64`, nunca el interno). Un literal fuera de rango produce exactamente un diagnóstico, sin cascadas.
- **Literal `u` usado como tipo con signo**: solo un **warning** (`unsigned literal used as signed type i64; remove the 'u' suffix`); compila igual.
- **Ensanchar sin cambiar el valor es silencioso**: mismo signo a mayor tamaño (`i8`→`i64`, `u8`→`u32`), o sin signo a con signo mayor (`u8`→`i64`). Vale en binarios, init, asignación, args y `return`, sin cast explícito (el IR inserta un `extend` al cambiar de ancho).
- **Todo lo demás entre tipos distintos es error** con sufijo `'as'` en el mensaje: estrechar (`i64`→`i8`), cambiar de signo (`i8`→`u64`, `u64`→`i64`), o mezclar dos variables de tipos distintos en un binario (`type mismatch: i64 vs u64 (use 'as' to convert explicitly)`). Mezclar un enum con un entero concreto o con otro enum tampoco es implícito.
- Operadores aritméticos y bitwise (`+ - * / % & | ^`): ambos operandos del **mismo tipo** (tras lo anterior), resultado del tipo de los operandos.
- Shifts (`<< >>`): el resultado es el tipo del operando **izquierdo**; el derecho puede ser cualquier numérico (`a << b` con `a : i64`, `b : u64` vale).
- Comparaciones (`< > <= >= == !=`): ambos operandos del **mismo tipo** (un literal flexible adopta el del otro: `c > 5` con `c : char` compara en `i8`); dos variables de tipos distintos son error. Dos valores del mismo enum sí se comparan (resultado `i64`). Nunca `void`. Resultado siempre `i64` (0 o 1).
- Lógicos binarios (`&&` `||`): aceptan operandos de **cualquier tipo no-`void` sin conversión**; resultado siempre `i64`.
- `-` y `~` unarios: preservan el tipo del operando. Negar una variable sin signo compila pero emite un **warning** (`negating an unsigned value; the result wraps`); negar un literal es silencioso (se pliega al valor exacto, también a través de un cast). `!` unario: acepta cualquier tipo no-`void`, resultado `i64`.
- **Cast explícito `expr as type`** (`type` = cualquier numérico, incluido `char` como alias de `i8`, un alias de tipo, un enum o un newtype): acepta cualquier operando no-`void` (incluido el resultado de otro cast) y devuelve exactamente el tipo escrito. Reinterpreta/trunca bits en silencio. Con enums, el cast explícito está permitido entre un enum y cualquier entero (incluido su subyacente), pero no entre dos enums distintos (ver §3.1). Con newtypes, el cast explícito está permitido entre un newtype y cualquier entero o enum, y entre un newtype de puntero y cualquier puntero, pero no entre dos newtypes distintos (ver §3.2). `as void` no existe (error de parser: `expected type`). Castear un `void` (p. ej. `f() as i64` con `f : void`) es error de sema (`cannot cast a value of type void`). Castear un operando con error (no declarado, string, etc.) no añade un segundo error: el cast propaga `error`. Un cast que involucre un puntero solo vale entre punteros (`p as ptr i64`, `p as ptr void`): ver el bloque **Punteros** más abajo.
- `void` no puede aparecer en expresiones (ni en condiciones, ni como operando, ni como init, ni como origen/destino de cast).

**Punteros.** `ptr T` con `T` = cualquier tipo entero, `enum`, `newtype`, alias de esos, `void`, **otro puntero** (`ptr ptr i64`) o una **firma de función** (`ptr fn(a : i32) : i32`, §8.2): la base se resuelve recursivamente (los alias se canonicalizan primero, así que `type P = ptr i64; var q : ptr P;` es `ptr ptr i64`) y `ptr` es idempotente sobre el mismo `T`. Un `ptr` de un tipo no resoluble o no válido es error (`invalid pointer base type 'X'`).

- **`$x` (dirección de)**: sobre una variable **escalar** (`NODE_ID`) — local, global o parámetro —, sobre una desreferencia (`$@p`, que devuelve el propio `p`) o sobre una indexación (`$p[i]`, que devuelve `p + i`). Da `ptr T` con el tipo del operando. Sobre un array o un slice en crudo es error: el array suelto ya decae a `ptr T` a su primer elemento, así que usa `a` directamente en lugar de `$a`. `$f` (función) es error (`'f' is a function; use it directly as a pointer value`) y cualquier otro operando también (`operand of '$' must be a variable, a dereference or an index`).
- **`@p` (desreferencia)**: `p` debe ser `ptr T`; da un *lvalue* de tipo `T`, así que vale como valor y como destino de asignación/`++`/`--`/compuesto (`@p = 7;`, `@p++;`, `@p += 1;`). `@pv` con `p : ptr void` es error (`cannot dereference a value of type ptr void`); `@x` con `x` no puntero, error. Desreferenciar un puntero sin asignar emite el warning normal de "usado sin asignar". Si en ese punto el puntero es conocido `NULL` (su init o su última asignación incondicional fue `NULL` y ningún camino vivo lo reasigna), `@p` emite un **warning** (`null pointer dereference: 'p' is NULL here`); el análisis es conservador y sensible al flujo, igual que el de "asignada" (§7).
- **`p[i]` (indexación)**: `p` debe ser `ptr T` (no `ptr void`) e `i` un valor entero, que se finaliza a su tipo por defecto. Es exactamente `@(p + i)`: un *lvalue* de tipo `T` con el offset escalado por `sizeof(T)` (en la IR, `p[i]` baja a `mul` por el tamaño del elemento + `add` + `ld_ptr`/`str_ptr`), así que vale como valor y como destino (`p[i] = 1`, `p[i]++`, `p[i] += 2`, `$p[i]`). Se encadena por la izquierda: `pp[i][j]` es `(pp[i])[j]`. `p[i]` con `p : ptr void` es error (`cannot index 'ptr void'; cast it to a typed pointer first`), indexar un no-puntero también (`cannot index a value of type T`) y un índice no entero también (`pointer indexing requires an integer index`). Un puntero conocido `NULL` emite el mismo warning que al desreferenciar. Un literal string **no** es indexable directamente (igual que no es desreferenciable): `"abc"[i]` es error. No se comprueban límites.
- **`NULL`**: es el único valor de tipo `nil`. Se convierte **en silencio a cualquier `ptr T`** (init, asignación, argumento, `return`) y se compara con cualquier puntero. No es un entero: `p == 0`, `p + 1` con `0`, `NULL as i64` y `5 as ptr i64` son error.
- **Aritmética**: `p + n` y `p - n` (con `n` de cualquier tipo entero, que se finaliza a su tipo por defecto) dan `ptr T`; `n + p` también. `p - q` con **el mismo** tipo puntero da `i64`: la distancia en **elementos**, como en C (`(p + 1) - p` vale 1; con `ptr i8` un elemento es un byte). Un array o un slice en cualquiera de los dos lados decae antes a su puntero de datos, así que `a + 1`, `1 + a`, `a - 1` y `a - b` valen (§3.3). Tipos distintos son error (`pointer type mismatch`), `ptr void` no admite ninguna aritmética (`pointer arithmetic on 'ptr void' is not allowed`) y un `ptr fn` tampoco (`pointer arithmetic on a function pointer is not allowed`, §8.2). El sema solo tipa; la IR aplica el **escalado** (`p + 1` avanza `sizeof(T)` bytes; también en `p - n`, `n + p`, `p++`/`p--` y `p += n`/`p -= n`) y la **desescalada** de `p - q` (dividiendo por `sizeof(T)`, con signo) mediante `ptr_scale` en `src/ir/expr.c`. Con `sizeof(T) == 1` el escalado se omite.
- **Comparaciones**: `==`/`!=`/`<`/`>`/`<=`/`>=` entre **el mismo** tipo puntero, o contra `NULL`. Resultado `i64`. Un literal string cuenta como `ptr i8`, así que `p == "abc"` vale con `p : ptr i8`. Los punteros a función solo admiten `==`/`!=` (y contra `NULL`): el orden relacional y la resta son error (§8.2).
- **Casts**: `expr as type` solo está permitido **entre punteros** (`p as ptr i64`, `p as ptr void`); `5 as ptr i64` y `p as i64` son error (`cannot cast between pointer and non-pointer types; only pointer-to-pointer casts are supported`). Un literal flexible **nunca** adopta un tipo puntero: `var p : ptr i32 = 7;` es error y hay que escribir `7 as ptr i32`.
- **`ptr void` ↔ `ptr T` es implícito, en las dos direcciones**: `ptr void` convierte **en silencio** a cualquier otro tipo puntero y cualquier `ptr T` convierte a `ptr void`, en init, asignación, argumento y `return`, sin `as`. Es el idioma para las funciones C de `void *`: `extern fn malloc (size : u64) : ptr void; var p : ptr i32 = malloc(40);` y `extern fn free (p : ptr void) : void; ... free(p);` compilan ambos, igual que `memcpy(dst, src, n)` con `dst : ptr void` y `src : ptr i8`. Un **array o un slice** también decaen a `ptr void` (además de a `ptr T` con el elemento como base, §3.3), así que `memcpy(dst, src, n)` acepta directamente `dst`/`src` de tipo `T[N]` o `T[]`. Dos punteros **no** relacionados (`ptr i32` → `ptr i64`) siguen siendo error (`use an explicit cast with 'as'`), y las **comparaciones** siguen exigiendo el mismo tipo puntero aunque uno sea `ptr void` (`p == q` con `p : ptr i64` y `q : ptr void` es error), porque un binario con punteros se resuelve en `check_pointer_binary`, antes de la conversión implícita.
- **`ptr const void`** (el `const void *` de C, §3.5) sigue las reglas de cualificación de C: **a** `ptr const void` convierte en silencio cualquier puntero, const o no (`ptr i8`, `ptr const i8`, `ptr void`); **desde** `ptr const void` se convierte en silencio a un pointee const (`ptr const T`, `ptr const i8`, ...) y a `ptr const void`, pero perder el const (`ptr const void` → `ptr void` o `ptr T`) exige `as`. Igual que `ptr void`, no admite desreferencia, indexación ni aritmética (los mensajes dicen `ptr void`).
- **Valores de verdad**: un puntero y `NULL` valen como condición y con `!`, `&&`, `||` (`NULL` es falso y advierte `condition is always false`). Un literal string **no** (`!"abc"`, `if ("abc")` son error). `-` y `~` sobre un puntero son error.
- **`sizeof`**: ver §5.
- **Globales puntero**: su inicializador debe ser una **constante de enlace**: `NULL` (o un cast de `NULL`), un literal string (que adopta `ptr i8`) o la **dirección de una global** (`$g`, también a través de un cast como `$g as ptr void`). Cualquier otra cosa es error (`global pointer initializers only support NULL, a string literal or the address of a global`). En `-O2`/`-O3` esos inits se ligan estáticamente en `.data` (`.quad 0`, `.quad .LCn`, `.quad __r2_g`) y `__r2_init` deja de escribirlos; en `-O0`/`-O1` se escriben en runtime durante `__r2_init`.

### 3.1 Enums

Un enum declara un **tipo nominal** entero con variantes con nombre:

```r2
enum Dias : u8 {
    Lunes = 0,
    Martes,        // 1
    Miercoles,     // 2
}
```

- Solo se declaran a nivel top-level; un `enum` dentro de una función o bloque es error (`enums can only be declared at the top level`).
- `: type` es opcional; el tipo subyacente por defecto es `u32`. Debe ser un entero concreto (`i8`, `u8`, `i16`, `u16`, `i32`, `u32`, `i64`, `u64` o `char` = `i8`); `void`, un tipo desconocido y otro enum se rechazan (`enum underlying type must be an integer type`).
- Un enum debe tener al menos una variante (`enum 'E' must have at least one variant`).
- La primera variante sin valor explícito vale `0`; cada variante siguiente sin valor explícito vale la anterior `+ 1`. Si el auto-incremento supera el máximo del tipo subyacente, es error (`enum variant value overflows the underlying type u8`).
- Un valor explícito solo puede ser un literal, `-literal`, `~literal` o un cast de una de esas formas: `0`, `-1`, `1u`, `(300 as u8)`, `~0`. Cualquier otra expresión (`A + 1`, un identificador, una llamada) es error (`enum variant value must be a literal, optionally negated, complemented or cast`).
- Los valores se evalúan con el plegado normal y se ajustan al tipo subyacente: `(300 as u8)` vale `44`, `(-1 as u8)` vale `255`; `(200 as u8)` con base `i8` es error de rango. `~0` pliega a `-1` (64 bits), así que vale para bases con signo y es error para unsigned; `~0u` da el máximo unsigned. Los errores de rango se reportan con el nombre del enum (`literal 200 does not fit in type Small`).
- Se permiten **valores duplicados** (alias estilo C), pero no **nombres de variante duplicados** (`duplicate enum variant 'Lunes'`).
- Se aceptan una coma final y un `;` opcional tras `}`.
- El nombre del enum es un nombre top-level normal: compite con variables/funciones en el chequeo de duplicados, no puede empezar por `__r2_` ni `_r2_` y no puede ser `main` (`'main' must be a function`).

**Tipado nominal.** `Dias` es un tipo propio, no un alias de `u8`. Se puede usar en cualquier posición de tipo (`var`, parámetros, retorno y casts). `Dias.Lunes` es una **constante de compilación** de tipo `Dias` (no un field load) y se puede usar en cualquier expresión. El tamaño y el signo del enum son los de su tipo subyacente, y el codegen lo trata como un entero de ese ancho.

- `var d : Dias = Dias.Lunes;` — sí.
- `var d : Dias = 1;` — sí: un literal flexible adopta `Dias` si su valor cabe en el tipo subyacente.
- `var u : u8 = 1; var d : Dias = u;` — no; hace falta `u as Dias`.
- `var d : Dias = ...; var u : u8 = d;` — no; hace falta `d as u8`.
- `Dias.Lunes + Dias.Martes` — sí; resultado `Dias`. También valen bitwise, shifts, comparaciones, `++`/`--` y asignación compuesta entre valores del mismo enum.
- `Dias.Lunes + 1` — sí; el literal adopta `Dias`.
- `Dias.Lunes + u8_var` — no; hace falta `as`. Mezclar un enum con un primitivo concreto o con otro enum nunca es implícito.
- `Dias.Lunes == Dias.Martes` — sí; resultado `i64`.
- `d as u8`, `u as Dias` — sí: el cast explícito está permitido entre un enum y cualquier entero (incluido su subyacente).
- `a as B` entre dos enums distintos — no (`cannot cast between enum types 'A' and 'B'; cast through the underlying integer type`).
- `Dias.Lunes = ...` — error (`left side of '=' must be a variable`); `Dias = ...` — error (`'Dias' is an enum type, not a value`).
- `Dias` usado como valor o como función — error (`'Dias' is an enum type, not a value` / `'Dias' is an enum type, not a function`).
- `Dias.Foo` con una variante inexistente — error (`enum 'Dias' has no variant 'Foo'`). El lado izquierdo del `.` debe ser un nombre cuyo tipo representado sea un enum: `a value of type T is not an enum type` si no es un identificador, `'X' is not an enum type` en caso contrario; si el nombre es una variable, esta tiene que ser un array o un slice (`'X' is not an array or a slice`). Vale un enum, un alias de enum y un newtype (o alias de newtype) sobre un enum; en los dos últimos el valor resultante tiene el tipo nominal (`newtype N = Dias; N.Lunes` es de tipo `N`, ver §3.2).

### 3.2 Aliases y newtypes

```r2
type Index = u32;          // alias transparente
newtype Seconds = u32;     // tipo nominal nuevo

type Day = Dias;           // D.Lunes es Dias.Lunes
newtype Stamp = Day;       // nominalmente distinto de Dias
```

- Solo se declaran a nivel top-level; dentro de una función o bloque es error (`types can only be declared at the top level`).
- El tipo de la derecha debe ser un tipo conocido: un primitivo (`i8`…`u64`, `char`), un enum, un puntero (`ptr T`), otro alias o otro newtype. Un nombre desconocido es `unknown type 'X'` y una referencia circular es `circular type definition 'X'`.
- Las declaraciones son **orden-independientes**: se puede usar un tipo antes de su declaración en el fichero, igual que los enums (`var x : Index = 1; type Index = u32;` compila).
- El nombre es un nombre top-level normal: compite con variables/funciones/enums en el chequeo de duplicados, no puede empezar por `__r2_` ni `_r2_` y no puede ser `main` (`'main' must be a function`).

**`type X = T` (alias transparente).** `X` no es un tipo nuevo: cualquier uso como tipo se canonicaliza a `T` y desaparece en sema, IR y codegen. Mismo criterio que `char` → `i8`.

- `type Index = u32; var x : Index = 1;` — `x` es de tipo `u32`; en `--dump-symbols` aparece `var x ... type=u32` y en los mensajes se ve `u32`, nunca `Index`.
- La declaración sí aparece en `--dump-symbols`: `type Index [l:c] depth=0 type=Index base=u32`.
- Cadenas: `type A = B; type B = u8;` → tanto `A` como `B` son `u8` (`base=u8` en el dump).
- `type D = Dias;` — `D` es exactamente `Dias`, así que `D.Lunes` funciona.
- `type A = N;` con `newtype N = u32;` — `A` es exactamente `N` (nominal, no `u32`).

**`newtype X = T` (tipo nominal).** `X` es un tipo distinto, con el mismo tamaño y signo que su representación física. Se comporta como un entero (o como un puntero, si la base es `ptr T`) de ese ancho para el codegen, pero mezclarlo con otros tipos sin `as` es error, igual que un enum.

- La base puede ser un entero concreto (`i8`…`u64`, `char`), un enum, un puntero (`ptr T`), un alias que acabe en uno de ellos, u otro newtype. `void`, los arrays, los slices, los structs y los tipos untyped no valen (`newtype underlying type must be an integer, enum or pointer type`).
- `newtype N = Seconds;` — `N` y `Seconds` siguen siendo distintos aunque compartan representación.
- `newtype N = E;` con `enum E : u8` — tamaño 1 y mismo signo que `E`. `N.A` es válido y produce un valor de tipo `N` con los bits de `E.A` (también a través de cadenas: `newtype P = N;` → `P.A`, y `type A = N;` → `A.A`). El valor es `N`, no `E`: `var e : E = N.A;` es error y exige `as`, igual que `E.A as N` para el sentido contrario.
- `var s : Seconds = 7;` — sí: el literal flexible adopta `Seconds` si cabe en su representación (`literal 300 does not fit in type Seconds` en caso contrario).
- `s + s`, `s + 1`, comparaciones, bitwise, `++`/`--` y asignación compuesta entre valores del mismo newtype — sí, con resultado `Seconds`.
- `u32 + s`, `u32 = s`, `s = u32_var`, argumento `Seconds` en parámetro `u32`, retorno `u32` desde función que devuelve `Seconds` — no; hace falta `as`. El mensaje es el genérico (`type mismatch: Seconds vs u32 (use 'as' to convert explicitly)`, `cannot assign ...`, etc.).
- Ensanchar un newtype a un entero mayor **no** es implícito (es nominal): `var u : u64 = s;` es error aunque `Seconds` sea `u32`.
- `s as u32`, `u as Seconds`, `e as Seconds` (enum → newtype) y `s as E` — sí: el cast explícito reinterpreta/trunca como entre enteros.
- `a as B` entre dos newtypes distintos — no (`cannot cast between newtype types 'A' and 'B'; cast through the underlying integer type`, o `... underlying pointer type` si son de puntero).
- `Seconds` usado como valor o como función — error (`'Seconds' is a type, not a variable` / `'Seconds' is a type, not a function`).
- En `--dump-symbols`: `newtype Seconds [l:c] depth=0 type=Seconds base=u32`.

**Newtypes de puntero.** `newtype P = ptr T` conserva el nominalismo del newtype y la semántica completa del puntero subyacente:

- Representación de 8 bytes. `sizeof(P) == 8`, `alignof(P) == 8`; el tag en `any` es el nombre del newtype (`"P"`), no el del puntero.
- `NULL` se asigna y se compara con `==`/`!=` sin `as` (`var p : P = NULL; if (p == NULL) ...`), igual que con `ptr T`. `!p` vale como condición.
- `@p`, `p[i]`, `p + n`, `n + p`, `p - q` (distancia en elementos), las comparaciones relacionales, `+=`/`-=`/`++`/`--` y el acceso a campos de `ptr struct` funcionan como en `ptr T`. El escalado usa `sizeof(T)`. El resultado de tipo puntero conserva `P` (`p + 1` es `P`); `@p`/`p[i]` dan `T` y `p - q` da `i64`.
- Sobre `ptr void` no hay desreferencia, indexación ni aritmética (igual que con `ptr void`), pero sí `NULL`, comparaciones y `as`.
- No hay conversión implícita en ningún sentido entre `P` y `ptr T`, ni entre dos newtypes (aunque compartan representación): hace falta `as`. `p as ptr T`, `raw as P` y `p as ptr void` valen; `p as i64` no (`cannot cast between pointer and non-pointer types`). `0` no inicializa un newtype de puntero, igual que no inicializa `ptr T`.
- Un global de tipo newtype de puntero admite los mismos inicializadores link-time constantes que `ptr T` (`NULL`, string, `$global`) y un struct/array global con un campo de newtype de puntero se inicializa desde `__r2_init`, no se liga a una imagen `.data`.

### 3.3 Arrays y slices

```r2
arr lista : i32[5] = {2, 4, 6, 8, 10};
arr nombre : i8[16] = "R2-Piler";   // copia + NUL + ceros al resto
arr matriz : i32[2][3] = {{1, 2, 3}, {4, 5, 6}};  // row-major: 2 filas de 3
arr cero : u8[4];                   // sin inicializar (stack o .bss)

fn suma(v : i32[]) : i64 {          // slice de rango 1: puntero + longitud
    var total : i64 = 0;
    var i : i64 = 0;
    while (i < v.len) {
        total += (v[i] as i64);
        i++;
    }
    return total;
}

fn total_matriz(m : i32[][]) : i64 { // slice genérico de rango 2
    var total : i64 = 0;
    var i : i64 = 0;
    while (i < m.len) {
        var j : i64 = 0;
        while (j < m[i].len) {       // longitud de la fila i (runtime)
            total += m[i][j] as i64;
            j++;
        }
        i++;
    }
    return total;
}

fn fila(v : i32[3]) : i64 {          // array fijo: se pasa solo el puntero
    return (v[0] as i64) + v.len;    // .len es la constante 3
}

pub fn main() : i64 {
    arr local : i32[2][3] = {{1, 2, 3}, {4, 5, 6}};
    var p : ptr i32 = local;         // decay al elemento hoja local[0][0]
    var q : ptr i32 = local[1];      // la fila 1 decae a &local[1][0]
    var n : u64 = local[1].len;      // constante flexible (3)

    return total_matriz(local) + (p[1] as i64) + (q[2] as i64) + (n as i64)
        + fila(local[0]);
}
```

- **Declaración**: `arr ID : T[N]… [= inicializador];` en locales y globales, con cualquier rango concreto. `arr` exige un tipo array: `arr x : i64;` es error (`'arr' declarations must have an array type`). Las dimensiones pueden dejarse como `[]` **si hay inicializador** y se deducen de él (`arr z : i32[] = {1, 2, 3};` es `i32[3]`, `arr m : i32[][] = {{1, 2}, {3, 4}};` es `i32[2][2]`; ver *Inicializadores*), mientras que un `arr z : i32[];` sin init sigue siendo error; la deducción es solo de `arr`, `var` no la aplica. Un `var ID : T[N]...` **local** declara una **vista** (ver *Vistas*), no almacenamiento; en top-level es error (`a view variable can only be declared inside a function; use 'arr' for a global array`) y `static var` con tipo array también (`a 'static' view is not supported; use 'static arr'`). Los slices (`T[]`, `T[][]`, ...) se declaran con `var` y son descriptores normales, no vistas.
- **Vistas**: `var v : T[N]...` con un tipo array (incluidas las formas mixtas `T[R][]`, ...) declara una vista **local no propietaria**, como un parámetro de ese tipo: guarda en el stack un puntero y una longitud `i64` por cada dimensión `[]` (`8 * (1 + dinámicas)` bytes) y **nunca copia** datos. Se inicializa o se asigna (`v = expr`) desde un array o slice del mismo rango y tipo hoja y con las mismas dimensiones escritas (la misma comprobación que un argumento, `sema_array_slice_compat`); `{...}` y los literales string están prohibidos (`a view cannot be initialized with a '{...}' list; ...`) porque el temporal moriría. Sin inicializador queda indefinida, como un slice. `.len` es constante en las dimensiones escritas y runtime en las `[]`; `sizeof` es el del array visto (o el del descriptor si hay `[]`); `$v` no vale; pasar la vista a un parámetro `T[N]`, `T[]` o mixto funciona como pasar el array, y la indexación y el decay a `ptr leaf` leen/escriben los datos originales. Asignar a la vista **re-apunta** (no copia) y no se permite sobre un `arr`. Retornar una vista de storage local avisa (`returning a pointer to the local 'v'`), igual que un slice.
- **Dimensiones**: cada `[N]` debe ser una expresión entera constante y mayor que cero; una variable (`i32[n]`) es `array size must be a constant integer expression`, `0` es `array size must be greater than zero` y un tamaño total que no cabe es `array is too large`. Las dimensiones se leen de fuera hacia dentro, como en C: `i32[2][3]` son 2 filas de 3 `i32` (row-major, contiguas) y se indexa `m[i][j]`; un array puede tener cualquier rango (`i32[2][3][4]`). Los slices pueden tener cualquier rango (`T[]`, `T[][]`, `T[][][]`, ...); cada `[]` vacío añade una dimensión al descriptor. Las dimensiones pueden **mezclarse**: `i32[R][]`, `i32[][C]`, `i32[R][][C]`, ... Las escritas son estáticas (constantes, comprobadas también en el índice constante) y cada `[]` es una longitud runtime; `sizeof` de un tipo mixto es el del descriptor, `8 * (1 + dinámicas)` (16 para `i32[2][]`, `i32[][3]` y `i32[2][][3]`; 32 para `i32[][][]`). No hay puntero-a-array: `ptr i32[2]` se interpreta como `ptr` a un array y se rechaza (`invalid pointer base type 'i32[2]'`); para un array de punteros se usan paréntesis, `(ptr i64)[2]`. El parser acepta como máximo 16 dimensiones en una especificación de tipo (`i32[1][1]...`); excederlo reporta `type has too many dimensions (max 16)` y sigue consumiendo sin construir más niveles.
- **Memoria**: los arrays `arr` locales viven en el stack y los globales en `.bss`; las vistas y los slices solo reservan su descriptor (puntero + longitudes) y apuntan a datos ajenos. Los slices también pueden ser globales y `static`, pero su descriptor (16 bytes para `T[]`, `8 * (1 + rank)` en general) **siempre** lo escribe `__r2_init` en runtime, aunque el inicializador sea constante (`var s : i8[] = "hi";`), porque `image_type_ok`/`bind_static` no aceptan slices. Un global con inicializador **totalmente constante** (sin punteros, slices ni bitfields) se liga a `.data`: los arrays y structs como imagen de bytes en **cualquier** nivel (IR-gen `bind_image`), y los escalares/punteros solo con el optimizador (`-O2`/`-O3`, `run_static_init`; en `-O0`/`-O1` se escriben desde `__r2_init`). Si el inicializador no es 100 % constante (una llamada, una dirección, un bitfield...), `.bss` queda a cero y `__r2_init` escribe los elementos con stores. Un global sin inicializador ya vale cero. Un array local sin inicializador queda indefinido, como un `var` sin init.
- **Inicializadores**: un `{...}` **posicional** debe ser **exhaustivo y exacto en cada nivel** (`array initializer must have exactly N elements (found M)`); no hay zero-fill implícito en la forma posicional. También se admite la forma **designada** `{[i] = valor, ...}` (solo designada: no se mezcla con posicionales ni con `.campo =`, y los índices pueden ir en cualquier orden): cada elemento se escribe en el índice indicado y **el resto se zero-inicializa** (el array entero se zeroa antes de las escrituras en locales; en globales y `static` el `.bss` o la imagen `.data` ya nace a cero). El índice es una expresión entera constante `>= 0` y menor que la longitud de esa dimensión; fuera de rango, repetido o solapado (un designador cuya ruta es prefijo de otro, p. ej. `[0]` y `[0][1]`) es error (`array initializer index N is out of bounds...`, `duplicate array initializer index`). Se permite el multi-índice `[i][j] = valor` (el valor inicializa el objeto que designa la ruta; si quedan dimensiones debe ser lista/string como en posicional) y anidar `{[1] = {[0] = 5}}`. Los valores siguen las reglas normales (listas, strings, literales de struct). En un `arr` con dimensiones `[]`, la forma designada participa en la deducción: cada nivel toma `máximo índice + 1` (también con multi-índice) y los valores anidados aportan sus longitudes. En un `arr` cuyo tipo deja dimensiones como `[]`, cada nivel deduce su longitud del número de elementos **directos** de su lista, así que `arr x : i32[3][] = {{1, 2}, {3, 4}, {5, 6}};` deduce `i32[3][2]` y `arr x : i32[][3] = {{1, 2, 3}, {4, 5, 6}};` deduce `i32[2][3]`; las listas deben ser rectangulares en cada nivel (el primer elemento fija la forma y el resto se comprueba como siempre). Un literal string en el `[]` hoja de `i8`/`u8` aporta `len + 1` bytes (`arr s : i8[] = "hola";` es `i8[5]`). Un `{}` en un `[]` deduce longitud 0, que es error (`array size must be greater than zero`). Un elemento de tipo array exige a su vez una lista `{...}` (`array element of type T requires a '{...}' initializer`) o, si su array hoja es `i8`/`u8`, un literal string; una lista donde el elemento es escalar es error (`array element of type T cannot be initialized with a '{...}' list`). Así, `i32[2][3] = {{1, 2, 3}, {4, 5, 6}}` e `i8[2][4] = {"ab", "cd"}`. Para `i8[]`/`u8[]` se admite un literal string en cualquier nivel: se copian los bytes más el terminador NUL y, en locales, el resto de esa fila se rellena con ceros; si `len + 1 > N` es error (`string literal needs ... but the array has room for ...`) y en un array que no sea `i8`/`u8` también (`string initializer requires an i8, u8 or char array`). Los elementos escalares siguen las reglas de asignación (literal flexible o ensanche silencioso); un valor no convertible es error en el elemento. Un `{...}` sobre un tipo no array es error (`'{...}' initializer requires an array type`).
- **`.len`**: en un array concreto se pliega al tamaño de **esa** dimensión y es una constante **flexible** (como `sizeof`), así que adopta el tipo del contexto si cabe (`var n : u64 = lista.len;`); sin contexto es `i64`. En `m : i32[2][3]`, `m.len` es 2 y `m[i].len` es 3 (constantes, independientes de `i`); en un array de rango 3 hay un `.len` por nivel. En un slice es un valor **runtime** y también **flexible**: `v.len` en `T[]` es la única longitud, y en un rango N cada nivel da su dimensión (`m[i].len`, `m[i][j].len`, ...; las matrices son rectangulares, así que no depende de los índices). Al ser runtime no se comprueba el rango en compilación: se lee al ancho del tipo que pide el contexto (`var b : u8 = v.len;` trunca a 8 bits, `var n : i32 = v.len;` a 32) y truncar queda a cargo del programador; sin contexto es `i64`. Es **solo lectura**: `lista.len = 3;` es error (`left side of '=' must be a variable`). En un array/slice, un campo distinto de `len` es `type T has no field 'X'`; sobre una variable que no sea array ni slice es `'x' is not an array or a slice`.
- **Decay a puntero**: un array de rango R usado en contexto de puntero (`var p : ptr T = a;`, asignación, argumento de tipo `ptr T`, `return`, llamada `extern`, `$a`, y los operandos de `+`/`-` con un puntero o un entero) decae a un `ptr leaf` **plano** al elemento hoja 0: para `m : i32[2][3]`, tanto `m` como `m[0]` dan `&m[0][0]` y `m[1]` da `&m[1][0]`. El paso es siempre `sizeof(leaf)`, no el de la fila, así que `m + 1` apunta a `m[0][1]` (no a `m[1][0]`); para avanzar una fila usa `m[1]`. No hay puntero-a-array ni copia por valor. `a[i]`, `a.len`, `sizeof(a)` y la conversión a slice se resuelven sobre el array **antes** de decaer. Un slice en contexto de puntero da su puntero de datos (también en `+`/`-`).
- **Índice**: `a[i]` es un *lvalue* exactamente como `p[i]`/`@(p + i)`, con el offset escalado por `sizeof(T)` (el elemento de esa dimensión). Se encadena por la izquierda: en `m : i32[2][3]`, `m[i]` tiene tipo `i32[3]` (se puede volver a indexar, pedir `.len`/`sizeof` o dejar que decaiga) y `m[i][j]` es el lvalue hoja. En un slice genérico `m : i32[][]`, `m[i]` tiene tipo `i32[]` y `m[i][j]` calcula `datos + (i * len_y + j) * sizeof(leaf)`, con `len_y` en runtime (`m[i].len`); la fila es contigua. El índice se finaliza a su tipo por defecto; un no entero es `array indexing requires an integer index` / `slice indexing requires an integer index`. Si el índice es una constante conocida, se comprueba en compilación **en ese nivel** (`index 3 is out of bounds for an array of size 3`, también para negativos); un índice variable **no** se comprueba (UB, como en C).
- **Aritmética**: en `+` y `-` el array (de cualquier rango) o el slice decae a su `ptr leaf` plano y la operación sigue las reglas de punteros: `a + i`, `i + a` y `a - i` dan `ptr leaf` con el offset escalado por `sizeof(leaf)` (para `m : i32[2][3]`, `m + 1` es `&m[0][1]`), y `a - b` (dos arrays/slices con el mismo tipo hoja) da la distancia en **elementos hoja** como `i64`. `a + b` es error (`cannot add two pointers`). El resto de operadores no acepta arrays: no se puede asignar un array entero (`a = b` → `cannot assign a value of type ...; arrays and slices must be modified element by element`; la excepción es una **vista**, que se re-apunta con comprobación de dimensiones, ver *Vistas*), ni usar `+=`, `++`/`--`, `-a`, `a * 2`, comparaciones o como condición (`operator cannot be applied to a value of type ...` / `expression with resulting type ... is not valid as a condition`). Se modifica elemento a elemento: `a[i] = ...`, `a[i] += ...`, `a[i]++`, `m[i][j] = ...`.
- **Parámetros**: `fn f(v : T[])` recibe un slice de rango 1 (puntero + longitud) y `fn f(m : T[][])` un slice genérico de rango 2 (puntero plano al elemento hoja + una longitud por dimensión: tres parámetros IR); en general un slice de rango N pasa `1 + N` parámetros. Pasar una vista `m[i]` a un parámetro de un rango menos pasa su puntero de fila y las longitudes restantes. En ambos la llamada acepta un array o un slice del **mismo rango** y **mismo tipo hoja** (`a slice parameter requires an array or slice of the same rank and element type`), y con un array las longitudes se toman de sus dimensiones (`sum(m)` con `m : i32[2][3]` pasa `&m[0][0]`, 2 y 3). Un parámetro puede además **mezclar dimensiones escritas y `[]`** (`i32[R][]`, `i32[][C]`, `i32[R][][C]`, ...): las escritas son **estáticas** (van en el tipo y se comprueban como en un array, incluido el índice constante fuera de rango) y cada `[]` es una longitud **runtime** que viaja en el descriptor tras el puntero, en orden (en general `1 + dinámicas` parámetros IR; `sizeof` del tipo es el del descriptor, `8 * (1 + dinámicas)`, o sea 16, 24, ...). Un tipo con dimensiones mezcladas vale como parámetro y como tipo de retorno (`: i32[2][3][]`); en `extern` sigue rechazado (`extern functions cannot take slice parameters`) y un **local** de ese tipo con inicializador `{...}` es error (`an array type with '[]' dimensions cannot be initialized with '{...}'`); un valor mixto se obtiene de otro parámetro/array mediante una vista `var m : T[R][] = ...` (§3.3, *Vistas*). En la llamada cada `[]` acepta cualquier longitud, pero cada `[N]` exige exactamente `N` (`a parameter with some unsized dimensions requires an array or slice of the same rank and element type, with the same size in every sized dimension`). `fn f(x : T[R][C])` recibe un array concreto de cualquier rango: se pasa únicamente el puntero plano al primer elemento hoja (no viaja ninguna longitud) y el array es modificable desde dentro, como en C; en la llamada el argumento debe ser un array de **exactamente** ese tipo (mismo elemento hoja y mismas dimensiones), así que cualquier otra cosa es error (`argument type ... does not match parameter type ...; an array parameter requires an array of exactly that length and element type`), incluidos un `ptr T`, un `T[]`, un `T[][]` o un `T[M]…` distinto. Dentro de la función `x.len`/`x[i].len` pliegan a las constantes de cada dimensión, `sizeof(x)`/`sizeof(x[i])` son los bytes de ese nivel y `x[i][j]` con índices constantes se comprueba igual que en un array local. Una función **sí** puede devolver un array o un slice: `: T[N]` devuelve solo el puntero y `: T[]` un descriptor (§8). Una función `extern` tampoco puede tomar slices de ningún rango (`extern functions cannot take slice parameters`), aunque sí arrays concretos `T[N]`/`T[R][C]`, que para C son un `ptr` al primer elemento; para las longitudes por separado se usan parámetros aparte.
- **`sizeof`**: `sizeof(a)` es el total de bytes del array (`R*C*…*sizeof(leaf)`), no 8; `sizeof(a[i])` es el tamaño de ese subarray y `sizeof(a[i][j])` el del elemento hoja. El de un slice es el descriptor: `8 * (1 + rank)` (16 bytes para `T[]`, 24 para `T[][]`, 32 para `T[][][]`) y `8 * (1 + dinámicas)` para un tipo mixto. `alignof` de un slice es siempre 8, porque el descriptor contiene un puntero (§5). Como cualquier `sizeof`, el resultado es una constante flexible. Ojo: `sizeof(i32[4])` y `sizeof(i32[])` valen los dos 16, y `sizeof(i64[2])` también; es una coincidencia, no una regla (`sizeof(u8[8])` es 8 y `sizeof(u8[])` 16).
- **Tipos de elemento**: entero, `char`, enum, `newtype`, alias, **struct/unión** de esos o un **puntero** (`(ptr i64)[2]`; con paréntesis, porque `ptr i64[2]` es “puntero a array” y se rechaza), en cualquier dimensión.

### 3.4 Structs y uniones

```r2
struct Punto { x : i32; y : i32; }
union Valor { i : i64; b : u8[8]; }

struct Cpu {
    tag : i8;
    union {
        whole : u32;
        struct { lo : u16; hi : u16; }
    }
}

var origen : Punto = {.x = 1, .y = 2};

fn suma (p : Punto) : i32 { return p.x + p.y; }

pub fn main () : i64 {
    var p : Punto = {.x = 3, .y = 4};
    var q : Punto = p;            // copia por valor
    q.x = 10;
    var v : Valor = {.i = 0};
    return suma(p) + q.x + (v.i as i64);
}
```

- **Declaración**: `struct Nombre { ... }` / `union Nombre { ... }` solo a nivel top-level. El nombre es obligatorio (`a top-level struct needs a name`), compite con variables/funciones/tipos/enums en el chequeo de duplicados, no puede empezar por `__r2_` ni `_r2_` y no puede ser `main` (`'main' must be a function`). Cada miembro es `campo : tipo;`, un **bitfield** `campo : tipo : ancho;` / `campo : uN;` / `campo : iN;` o una declaración anidada **anónima** `struct { ... }` / `union { ... }` (sus campos se aplanan dentro del padre, con los offsets sumados); una declaración anidada **con nombre** se parsea pero sema la rechaza (`a named struct cannot be declared inside another struct`). Un struct/union vacío es error (`struct 'X' must have at least one field`) y un campo no puede contener al propio struct ni directa ni a través de arrays: `field 'x' has incomplete type 'X'; a struct cannot contain itself, use a pointer`. Los **slices sí valen como campo** (`s : i32[];`): ocupan el descriptor (`sizeof` 16, `alignof` 8) y se copian como cualquier campo. Nombres de campo duplicados, incluidos los aplanados, son error (`duplicate field 'x'`). Los structs y las uniones son tipos nominales: dos structs con los mismos campos son tipos distintos.
- **Bitfields**: `x : u8 : 3` es un campo de 3 bits en una unidad de 1 byte. Forma corta `uN`/`iN` (`u3`, `i5`, ..., `u63`, `i63`): el holder es el entero estándar más pequeño de esa signedness con ancho `>= N` (`u1..u7` → `u8`, `u9..u15` → `u16`, `u17..u31` → `u32`, `u33..u63` → `u64`); los anchos 8/16/32/64 se escriben en la forma explícita `u8 : 8`, `i16 : 16`, ... porque `u8`/`i8`/... son keywords. El tipo explícito debe ser entero (`i8..u64`, `char`, alias; `bitfield type must be an integer type`), el ancho un literal con `1 <= N <= 8 * sizeof(T)` (`bitfield width must be between 1 and 64`, `bitfield width N does not fit in type T (M bits)`), y no hay bitfields anónimos de relleno ni arrays/punteros de bitfields. Un bitfield es un *lvalue* de su **tipo holder**: se lee y se escribe como cualquier entero (aritmética, condiciones, argumentos, `return`, casts, comparaciones), `$` es error (`cannot take the address of bitfield 'x'`), `sizeof`/`alignof` dan los del holder y `offsetof` lo rechaza (`offsetof cannot be applied to bitfield 'x'`). Asignar una constante que cabe en el holder pero no en el ancho avisa (`value V does not fit in a N-bit bitfield; it will be truncated`).
- **Layout**: C-like, en orden de declaración. Cada campo empieza en el siguiente offset alineado a la alineación natural del campo (los arrays usan la alineación de su elemento hoja; un campo slice alinea a 8) y el tamaño del struct se redondea al máximo align; una unión pone todos sus miembros en el offset 0, con tamaño = máximo tamaño y align = máximo align. Ejemplo: `struct Inner { a : u8; b : u32; }` mide 8 con align 4; `struct Outer { c : i8; i : Inner; v : u16[3]; }` mide 20 con align 4. La alineación de un struct es la de su miembro más alineado. Los bitfields empaquetan en unidades de su holder: se aprovechan los bits libres mientras el siguiente quepa y, al no caber (o al aparecer otro tipo de holder o un campo normal), se abre una unidad nueva alineada al tamaño del holder; la unidad completa cuenta para el tamaño aunque queden bits sin usar; en una unión cada bitfield es una alternativa en el offset 0. `packed struct` / `packed union` ponen `align = 1` y quitan todo el padding (incluida la alineación de las unidades de bitfield): `packed struct H { tag : u8; value : u32; }` mide 5 con align 1. `packed` solo puede preceder a una declaración top-level (`a nested struct cannot be declared packed`), se propaga a los structs/uniones anónimos anidados y no existe como tipo (`type P = packed struct ...` no se escribe). Los accesos no alineados son válidos en x86-64.
- **Inicializadores**: designados `{.campo = valor, ...}`. Se puede **omitir campos** (inicialización parcial): los omitidos se zero-inicializan, así que `var p : P = {.x = 1}` deja `y` a cero y `var p : P = {};` zero-inicializa el struct entero; en una unión se elige exactamente una alternativa (`field 'x' overlaps another initialized member of the same union`, `missing a member of the union that contains ...`) y `{}` deja la unión a cero. Los arrays **posicionales** siguen siendo **exhaustivos** (`array initializer must have exactly N elements (found M)`), aunque cada elemento struct puede ser parcial (`{{.x = 1}, {}, {.y = 2}}`); las listas designadas de array (§3.3) pueden omitir elementos y se zero-rellenan. El zero-fill se emite antes de los campos escritos, así que un campo puede leer el valor anterior del destino (`p = {.y = 5, .x = p.y} as P`). No hay inicializadores **posicionales**: mezclarlos con designados es error de parser (`cannot mix '.field = value', '[index] = value' and positional initializers`) y un `{...}` posicional sobre un struct es `positional '{...}' initializers are not valid for struct 'X'; use '.field = value'`. Un campo desconocido es `struct 'X' has no field 'y'` y uno duplicado `duplicate field 'y' in the initializer`. Un campo struct anidado usa otro `{...}` designado, un campo array usa `{...}`/string (reglas de §3.3) y un campo escalar sigue las reglas de asignación (literal flexible o ensanche; si no, `cannot initialize field ...`).
- **Literales de struct generales**: la forma con tipo explícito `{...} as T` (o `{} as T`) es un literal de struct/unión válido en cualquier posición de expresión: `return {.x = 1, .y = 2} as P;`, `p = {.y = 3} as P;`, `f({.x = 7} as P)`, elemento de un array de structs, valor de un campo de otro literal. Sin `as`, un `{...}` en posición de expresión es `a '{...}' literal needs an explicit type; write '{...} as T'`. El `as` sobre un `{...}` es una anotación de tipo, no una conversión: no emite el warning de cast redundante. El literal es un objeto nuevo: si el destino ya existía, se materializa en un temporal y se copia después, así que los campos leen el estado anterior del destino. En la declaración de una `var`/`arr`/`static` el tipo ya está en el nodo y `var p : P = {.x = 1};` no necesita `as` (`var p : P = {.x = 1} as P;` también vale). Un literal de struct no se puede usar en `?:`, comparaciones ni aritmética, y `sizeof({...} as P)` no es válido (usa `sizeof(P)`). Un `{...}` al principio de un statement se parsea como bloque: para usarlo como sentencia hay que parentetizarlo, `({.x = 1} as P);`.
- **Acceso**: `p.campo` da un *lvalue* del tipo del campo (también en el lado izquierdo de `=`, compuesto, `++`/`--`, `$` y `sizeof`). `ptr.campo` sobre un `ptr struct` desreferencia automáticamente (`(*ptr).campo`) y emite el warning de `NULL` conocido; `arr[i].campo` funciona sobre arrays/slices de structs. Un campo inexistente es `struct 'X' has no field 'y'`. `.len` sigue siendo exclusivo de arrays y slices. Un struct no es valor de verdad ni admite comparaciones, aritmética ni casts (salvo a través de un tipo puntero: `p as ptr Punto`).
- **Copia y paso**: `=`, el paso de argumentos y el `return` copian el struct **por valor** (bytes y padding); modificar la copia no afecta al original. Un struct de hasta 16 bytes viaja en 1 o 2 registros de 64 bits (parámetros `u64` y retorno en `rax`/`rdx`); uno mayor se pasa por puntero a una copia (convención interna) y un retorno grande usa sret, todo transparente en el lenguaje. `packed` no cambia la ABI (mismo criterio por tamaño) y los bitfields se copian como parte de su holder. No se puede asignar un array de structs entero (valen las reglas de arrays de §3.3) ni devolver un array.
- **Memoria y `sizeof`**: un struct local vive en el stack, uno global/`static` en `.bss`. Un global/`static` con inicializador **totalmente constante** (sin punteros, slices ni bitfields) se liga a `.data` como imagen de bytes; en caso contrario se escribe campo a campo desde `__r2_init`, incluido el zero-fill de un inicializador parcial. `sizeof(struct)` es el tamaño del layout y **`alignof(struct)` es su align** (§5). `--dump-symbols` imprime `struct X ... size=N align=A` (o `union`, con ` packed` si aplica) y una línea `field nombre type=T offset=N` por campo (incluidos los aplanados, con ` bits=N bit_off=M` si es un bitfield).
- **`any` (valores heterogéneos)**: `any` es un struct builtin de 16 bytes (`tag : ptr i8` en el offset 0, `value : i64` en el 8, align 8) pensado para colecciones de valores de tipos distintos. Cualquier **escalar** (los ocho enteros, `char`), un **enum**, un **newtype**, un **puntero**, un **string** o **`NULL`** se empaqueta en silencio en un `any` en init, asignación, argumento, `return` y campo de struct (`var a : any = 41;`, `var b : any = NULL;`), sin `as`. El empaquetado guarda en `value` el valor canónico de 64 bits (extendiendo los tipos más estrechos) y en `tag` un puntero al nombre del tipo **original** (`"i64"`, `"u8"`, `"ptr i8"`, `"string"`, `"nil"`, `"ptr fn(i32) : i32"`, el nombre de un enum/newtype...). `a.tag` es un `ptr i8` que apunta al nombre **canónico** del tipo, así que `a.tag == "i8"` compara con el literal string; los alias no cambian el tag (se canonicalizan), pero un `newtype` conserva el suyo. Un `any` se desembala con un **cast explícito** `a as T`; T puede ser cualquier tipo empaquetable y el resultado es el valor guardado (truncado al ancho de T si es más estrecho), **sin comprobación de tipo** en runtime: `a as i64` sobre un `any` que guarda un `u8` reinterpreta el payload. No se empaqueta un array, un slice, un struct ni un `any` (un `any` se copia tal cual), y un cast a/desde un struct sigue siendo `cannot cast to or from a struct type`. Un `any` en un global/`static` no se liga a `.data` (contiene un puntero) y se inicializa desde `__r2_init`. `.tag`/`.value` son de solo lectura por sí; se modifican asignando el `any` entero.
- **Tipos**: `struct`/`union` se pueden usar en `var`, `arr`, parámetros, retorno, punteros y alias (`type A = Punto;`); no hay conversión implícita entre structs distintos ni con otros tipos. El cast explícito entre punteros (`p as ptr Punto`, `p as ptr void`) sigue las reglas de §3. **Limitación conocida**: un alias cuyo destino es un array de structs (`type SA = S[2];`) falla con `array element type cannot be void`, porque los alias se resuelven antes de que el struct tenga layout; se evita usando una variable/parámetro de tipo `S[2]` o declarando el array con `arr`/`var` directamente.

### 3.5 `const`

`const` es un **calificador de tipo**, no un prefijo de declaración: se escribe en el tipo y se aplica al **átomo de tipo** que sigue.

```r2
var x : const i64 = 3;          // objeto const
var p : ptr const i64 = $x;     // puntero a i64 const: @p no se puede escribir
var q : const ptr i64 = $g;     // puntero const: q no se puede reasignar, @q = ... sí
arr a : const i32[2] = {1, 2};  // array cuyos ELEMENTOS son const i32
```

- **Dónde se pega**: `const` califica al siguiente átomo **antes** de los sufijos `[...]`. Por eso `const T[N]` es `(const T)[N]` —un array de elementos const, así que `v[i] = ...` se rechaza— y `const (T[N])` es equivalente: los arrays y los slices son const a través de sus elementos (no existe la “vista const” independiente; una forma para eso queda pendiente). En punteros, `ptr const T` es “puntero a `T` const” y `const ptr T` es “puntero const a `T`”, igual que en C leído de derecha a izquierda. Repetir el calificador (`const const T`) no es error: se colapsa. La sintaxis antigua (`const var x : T`, `const arr a`, y `const` delante de un parámetro o del tipo de retorno) ya no existe: es error de parser con el hint `write the type as 'var x : const T'`.
- **Tipos que no admiten `const`**: `void`, `any` y una firma `fn(...)` desnuda son error al resolver el tipo (`'const' cannot be applied to type void; only object types can be const`). La única excepción es `void` **detrás de un puntero**: `ptr const void` es válido (el `const void *` de C, p. ej. el `src` de `memcpy`) porque el pointee no es un objeto que se lea o escriba; `const void` como valor, parámetro o retorno sigue siendo error. Sí valen enteros, `char`, enums, `newtype`, alias, structs/uniones, punteros, arrays, slices y `const ptr fn(...)` (el puntero a función sí es un objeto).
- **Inicialización obligatoria**: una `var`/`arr`/`static` cuyo tipo es const **necesita inicializador** (`const variable 'x' requires an initializer`, también para arrays); las declaraciones `extern` y las importadas desde `.r2i` están exentas.
- **Escrituras**: el tipo de un *lvalue* const es de solo lectura. Se rechazan `x = ...`, `x += ...`, `x++`, `@p` con `p : ptr const T`, `p[i]` con `p : ptr const T`, un campo declarado `f : const T` (`s.f = ...`), y `q = ...` cuando `q : const ptr T` (el objeto puntero es const), aunque `@q = ...` sí vale (el pointee no lo es). Mensajes: `cannot modify const variable 'x'` / `cannot modify const parameter 'x'` cuando el objeto raíz es const, y `cannot modify a const value of type T` cuando el const viene del tipo del lvalue (pointee, campo, elemento).
- **Propagación (como C)**: un miembro de un valor `const S` es de solo lectura aunque el campo esté declarado sin `const` (`var s : const S; s.x = 1;` es error), y un parámetro `const S` no puede modificar sus campos. Los elementos de un array/slice de elementos const se comportan igual.
- **Lectura**: leer un objeto const da un **rvalue sin const** (conversión lvalue-to-rvalue de C): `var y : i32 = x;` con `x : const i32` vale, y `x + 1` tiene tipo `i32`. Las operaciones (aritmética, bitwise, shifts, comparaciones, `-`, `~`, `!`) nunca propagan el const externo. **El const anidado no se descarta nunca al leer**: `ptr const i32` sigue siendo “puntero a const” como valor.
- **Conversiones implícitas**: añadir const es implícito (`i32` → `const i32`, `ptr i32` → `ptr const i32`, `i32[4]`/`i32[]` → `const i32[4]`/`const i32[]`, y un lvalue no-const como argumento de un parámetro `&const T`); **quitar const nunca es implícito**: pasar `p : ptr const i64` a una `var` de tipo `ptr i64`, o a `ptr void`, es error (`use an explicit cast with 'as'`). El cast explícito `x as T` sí puede quitar const. Las comparaciones y la resta de punteros aceptan el mismo pointee salvo por su const (`p == cp` vale).
- **Firmas de función**: el const **externo** de un parámetro o del valor de retorno no forma parte de la firma `ptr fn(...)`: `fn f(x : const i32) : const i32` tiene tipo `ptr fn(i32) : i32` y una variable de ese tipo se puede llamar con cualquier `i32`. El const **anidado** sí cuenta (`ptr const i32` en la firma es distinto de `ptr i32`). En el cuerpo, el parámetro const sigue siendo de solo lectura.
- **Interfaces**: el `.r2i` imprime el calificador tal cual (`pub var g : const i64;`, `items : const i32[2]`, `q : const ptr i64`). La cabecera C lo traduce a C: `const int64_t f;`, `const int32_t items[2];`, `const int64_t * p;` y `int64_t * const q;`.
- **`sizeof`/`alignof`**: aceptan `const T` (`sizeof(const i64)` es 8).

---

### 3.6 Memoria dinámica: `new` y `delete`

```r2
import "stdlib";                       // el new/delete por defecto usan malloc/free

var a : i32[]   = new i32[60];         // slice i32[]: malloc(60 * sizeof(i32))
var m : i32[][] = new i32[3][4];       // slice i32[][]: 3*4 i32 contiguos y 2 longitudes
var p : ptr i32 = new i32;             // ptr i32: malloc(sizeof(i32))
var b : i32[]   = new(my_alloc(240)) i32[60];  // allocator propio (no llama a malloc)

delete a;                              // free del puntero de datos
delete(mi_free) m;                     // llamada indirecta al deallocator
```

- **Formas**: `new T` reserva `sizeof(T)` bytes y da un `ptr T`; `new T[N]` da un slice `T[]`; `new T[D1][D2]...` da un slice de rango N (`T[][]...`) con una longitud runtime por dimensión. El tamaño reservado es `sizeof(T)` para `new T` y `sizeof(T) * D1 * D2 * ...` en el resto. Cada `[D]` es una expresión entera **runtime** (no hace falta que sea constante); una dimensión vacía es error de parser (`expected the allocation size after '['`). La memoria no se inicializa (semántica de `malloc`), así que su contenido es indefinido hasta que se escriba.
- **Tipo base**: es un **átomo de tipo** (§3), no una especificación con sufijos: el primer `[...]` que sigue al átomo es una dimensión de la reserva, no parte del tipo base. Los paréntesis **inmediatamente** tras `new` son el allocator (`new(expr) ...`); los que siguen al tipo son argumentos de un constructor de clase (`new P(args)`, §3.7), así que un tipo base que necesite paréntesis en la posición del allocator se declara con un alias: `type P = ptr i32; new P[4]`. No se puede reservar `void` ni un tipo de tamaño cero (`cannot allocate a value of type T with 'new'`), ni un tipo con rank dinámico (`type Slice = i32[]; new Slice[3]` es error: el elemento sería un slice). El tipo base se resuelve como en un cast, así que admite nombres cualificados (`m::T`), structs, uniones, enums, `newtype`, alias, punteros y arrays concretos.
- **Tipo del resultado**: un slice (`T[]`, `T[][]`, ...) si hay al menos una dimensión y un `ptr T` si no la hay. Se comporta como cualquier slice: `.len` por nivel, indexación `x[i][j]`, decay a `ptr leaf`, paso a parámetros `T[]...`, `for` por índice y `sizeof` del descriptor. `var x = new i32[4];` infiere `i32[]` y `var p = new i32;` infiere `ptr i32`.
- **Allocator por defecto**: `new ...` sin allocator (incluido `new P(args)`) y `delete x;` sin deallocator exigen que el fichero importe el módulo `stdlib` (cualquier `import "stdlib";`, `import "stdlib" as c;`, `from "stdlib" import ...` o una ruta que acabe en `/stdlib`); si no: `'new' requires the R2 standard library; add 'import "stdlib";'` (y el equivalente `'delete' ...`). El compilador emite llamadas directas a los símbolos `malloc`/`free` de libc.
- **Allocator personalizado**: `new(expr) T[N]...` exige que `expr` sea un **puntero** (`the allocator expression of 'new' must be a pointer, not i32`); no se llama a `malloc`, sino que ese puntero se usa como datos (el usuario calcula el tamaño dentro de `expr`, p. ej. `new(mi_arena_alloc(6 * sizeof(i32))) i32[6]`). Las dimensiones solo se evalúan para guardar las longitudes del slice. Estas formas **no** exigen `import "stdlib"`. Si `T` es una clase y no hay dimensiones, `expr` debe ser `ptr T` o `ptr void` (placement, §3.7).
- **Constructores de clase**: `new P(args)` reserva con `malloc` y llama al `init` de `P` (§3.7); `new(slot) P(args)` ejecuta el `init` sobre la memoria de `slot` (que debe ser `ptr P` o `ptr void`) sin llamar a `malloc` y devuelve el mismo puntero. `new P` sin paréntesis **no** llama a `init` (solo reserva, como cualquier `new T`); `new P()` sí lo llama con cero argumentos/defaults. Sin `init`, `new P()`/`new(slot) P()` solo reservan y `new P(args)` con argumentos es error. `new P[N]` sigue siendo una reserva de un slice de clases sin construir; `new P[N](args)` no está soportado. El `new P(args)` por defecto exige `import "stdlib";`; la forma placement no.
- **`delete`**: `delete x;` acepta un `ptr T` (libera ese puntero), un slice (libera su **puntero de datos**, nunca el descriptor) o `NULL` (`free(NULL)` es válido); cualquier otra cosa es `'delete' requires a pointer or a slice, not T`. Si `T` es una clase con `destroy`, el destructor se llama **antes** de `free`/del deallocator. La decisión es por tipo canónico: también se destruye un `ptr Clase` que vino de `new Clase` sin paréntesis (sin `init`), y no se comprueba `NULL` en runtime (un `delete p` con `p == NULL` y destructor peta; sin destructor, `free(NULL)` sigue siendo válido). Un `delete` de un slice de clases **no** destruye los elementos: solo libera la memoria (esos elementos pueden no estar construidos). La forma `delete(expr) x;` exige que `expr` sea un puntero a función con **exactamente un parámetro puntero** (el operando debe convertir a él en silencio) y retorno `void` o escalar (struct, slice, array y multi-retorno están rechazados); se emite una llamada indirecta con el puntero de datos como único argumento y el valor de retorno se descarta. Si el operando empieza por `(`, `delete(x);` es el mismo `delete x;`; solo cuando hay otra expresión detrás del paréntesis se interpreta como deallocator (`delete(mi_free) x;`). `delete` sobre un parámetro (incluido `self`) es error: los parámetros son prestados.
- **Globals y `static`**: `new` y los constructores de clase (`P(args)`) también valen en inicializadores de globales y de `static var`/`static arr`, que se ejecutan una vez desde `__r2_init` (el módulo queda con `has_dynamic_init`). Son la excepción a la regla de "inicializador constante" de `static`, pero no pueden referenciar variables locales (`a 'static' initializer with 'new' cannot reference locals`). Un global `ptr` inicializado con `new` es, por tanto, init dinámico y no una constante de enlace.
- **Propiedad**: no hay seguimiento de propiedad ni destrucción automática al salir de un scope. `delete` libera con `free` (o el deallocator indicado) y, si el tipo es una clase con `destroy`, antes llama al destructor. Un binding declarado con `new(allocator)` (placement) queda marcado: `delete p;` sobre él es error y hay que usar `delete(deallocator) p;`. `drop`/`reset` (§3.7) son las formas explícitas de destruir sin liberar y de reinicializar. Liberar un slice que apunta al stack, liberar dos veces, usar memoria tras `delete` o destruir dos veces es comportamiento indefinido (§12).

### 3.7 Clases y métodos

Una **clase** es un struct con funciones miembro, un **inicializador** y un **destructor** opcionales. De momento no hay `op`: una clase se construye con `Clase(args)` (o se deja sin inicializar), nunca con un literal `{...}`.

```r2
class Caja {
	x : i64;

	init (self, x : i64) { self.x = x; }

	destroy (self) { }

	priv fn base(self) : i64 { return self.x; }

	fn add(self, v : i64) : void { self.x = base(self) + v; }

	fn doble(x : i64) : i64 { return x * 2; }   // asociada: Caja::doble(21)
}

pub fn main () : i64 {
	var c : Caja = Caja(1);
	c.add(4);
	var p : ptr Caja = $c;
	p.add(2);
	return c.x + Caja::doble(3) + p.doble(4);
}
```

- **Declaración**: `class Nombre { ... }` solo a nivel top-level. El nombre es obligatorio y compite con structs, uniones, enums, tipos, variables y funciones. Una clase es un tipo `TK_CLASS` con el mismo layout y la misma ABI que un struct: admite campos con tipo, bitfields, structs anidados, `sizeof`, `alignof`, copia por valor, punteros, arrays y un `init` opcional. Debe tener al menos un campo (`class 'X' must have at least one field`). Los literales `{.campo = valor}` y `{...} as Clase` están **prohibidos** para clases (§3.7).
- **Métodos con `self`**: `fn nombre(self, ...)` declara un método. `self` se escribe **sin tipo**, solo como **primer** parámetro y solo dentro de una clase; su tipo implícito es `ptr Clase` (mutable), así que `self.campo` escribe la instancia con el auto-deref normal de punteros a struct. `self` no admite valor por defecto y no dispara el warning de parámetro no usado. En cualquier otra posición (`fn` normal, segundo parámetro, `self : T`) es error (`'self' is only valid as the first parameter of a method`, `'self' must be the first parameter of a method`, `'self' has an implicit type and cannot declare one`).
- **Inicializador `init`**: se declara dentro de la clase **sin** `fn` y **sin** tipo de retorno: `init (self, ...) { ... }`. `self` es obligatorio y debe ser el primer parámetro; no hay sobrecargas (un solo `init` por clase). Sus parámetros admiten lo mismo que los de un método normal (defaults, argumentos con nombre, `&T`, variádicos tipados) y el cuerpo puede usar `return;`. Un `init` no es un método llamable: `p.init(...)`, `Clase::init(...)` y `init(...)` dentro de la clase son error (`the initializer of class 'X' cannot be called directly; use 'X(...)'`). No puede ser `priv`, ni asociado (sin `self`), ni declararse con `fn` (`fn init(self)` es error). `init` y `destroy` solo son especiales como miembros de una clase; fuera de ella son identificadores normales (`var init : i64;` y `fn init` top-level valen). `drop` y `reset` sí son keywords del lenguaje. Un campo de la clase no puede llamarse `init` ni `destroy`.
- **Destructor `destroy`**: se declara dentro de la clase **sin** `fn` y **sin** tipo de retorno: `destroy (self) { ... }`. `self` es obligatorio y debe ser el único parámetro (nada de parámetros extra, defaults ni variádicos); no puede ser `priv` y hay uno por clase (`class 'X' already has a destructor`). El cuerpo admite `return;` y puede llamar a otros métodos, incluidos los `priv`. El destructor **no** se puede llamar directamente: `p.destroy()`, `Clase::destroy(...)`, `destroy(...)` dentro de la clase y usar `Clase::destroy` como valor son error; solo lo invocan `drop`, `delete` y `reset`. `fn destroy(self)` también es error (`the destructor ... must be declared as 'destroy (self) { ... }' without 'fn'`).
- **Construcción `Clase(args)`**: una llamada a un nombre cuyo tipo canónico es una clase construye un valor. Se acepta en cualquier posición de expresión (inicializador de `var`, `return`, argumento, campo de struct, elemento de array, ternario, etc.) y también en globales y `static`. Los argumentos se chequean contra los parámetros de `init` **sin contar `self`**; sin `init`, `Clase()` es válido (el objeto queda sin inicializar) y `Clase(args)` con argumentos es error. Si `init` existe, `Clase()` también lo llama (con defaults si los hay). La construcción en un destino fresco (`var p : Clase = Clase(args)`, global, `static`, `return Clase(args)`, elemento de array o campo de un literal) ejecuta `init` **directamente sobre ese destino** (recibe su dirección); en el resto se construye en un temporal y se copia por valor. No se pone nada a cero: los campos que `init` no escriba quedan indefinidos. Un alias transparente (`type A = Clase; A(args)`) vale; `newtype` sobre una clase no está permitido (el tipo subyacente de `newtype` sigue limitado a entero, enum o puntero).
- **`new` y almacenamiento**: `new Clase(args)` reserva con `malloc` y llama a `init`; `new(slot) Clase(args)` llama a `init` sobre `slot` (que debe ser `ptr Clase` o `ptr void`) sin `malloc`, y devuelve el mismo puntero. `new Clase` sin paréntesis **no** llama a `init`; `new Clase()` sí. `new Clase[N]` no construye los elementos y `new Clase[N](args)` no está soportado. Un binding declarado con `new(allocator)` queda marcado como placement: `delete p;` sobre él es error y hay que usar `delete(deallocator) p;`.
- **`drop` y `reset`**: son sentencias, no expresiones. `drop expr;` llama al `destroy` de la clase y **no libera memoria**; `expr` puede ser un valor de clase lvalue (variable, campo, `a[i]`, `@p`) o cualquier expresión `ptr Clase` (incluida `make()`; `drop` sobre un puntero de `new` deja la reserva sin liberar, fuga intencional). Exige que la clase tenga destructor (`class 'X' has no destructor; 'drop' is not available`); sobre un parámetro o `self` es error (son prestados) y sobre un target `const` también. `reset target(args);` exige un target lvalue (`variable`, campo, `a[i]` o `@p`) de tipo clase o `ptr Clase`, que la clase tenga **destructor e inicializador**, y hace `destroy` y después `init` sobre el mismo slot (la dirección se evalúa una sola vez); los argumentos se chequean contra `init` sin `self` (defaults, argumentos con nombre, `&T` y variádicos tipados incluidos). Sobre parámetros y `self` sí vale (termina con el objeto inicializado); el target no puede ser `const`. Ninguna de las dos comprueba `NULL` en runtime: un target nulo con destructor peta, como en C.
- **Literales y `destroy`**: `{.campo = valor}` y `{...} as Clase` son siempre error (`class 'X' cannot be initialized with '{...}'; use 'X(...)'`), también para un campo de tipo clase dentro del literal de un struct (`{.c = {...}}`); en ese campo sí se puede usar una expresión constructora (`{.c = Clase(...)}`). Omitir un campo de clase en un literal de struct sigue rellenándolo a cero como ahora. Un campo de la clase no puede llamarse `destroy`; el destructor solo existe en la forma especial `destroy (self) { ... }` (ver arriba).
- **Llamadas**: `p.m(args)` sobre un valor clase o un `ptr Clase` inserta el receptor como primer argumento de `m`. Sobre un valor se pasa su dirección; sobre un puntero se pasa el puntero. Vale cualquier receptor, incluidos temporales (`make().m()`, `arr[i].m()`, `s.campo.m()`). Dentro de la clase, `m(args)` resuelve primero a un método de la clase actual (oculta una función global del mismo nombre). `Clase::m(args)` siempre vale como llamada directa; para un método con `self` hay que pasar el receptor explícitamente (`Caja::add($c, 4)`). El `init` no se llama directamente (ver arriba); `Clase(args)` es la única forma de construirlo en stack.
- **`priv`**: `priv fn m(...)` es visible solo desde métodos de la misma clase (cualquier instancia). Desde fuera, una llamada `p.m()`, `Clase::m(...)` o el uso de `Clase::m` como valor es error (`method 'Clase::m' is private`). `priv` vale también para funciones asociadas.
- **Nombres**: el campo y el método de una clase no pueden llamarse igual (`class 'X' already has a field named 'm'`) y no puede haber dos métodos con el mismo nombre (`'X::m' already declared in this scope`). Un método normal no puede llamarse como su clase (`a method cannot have the same name as its class`). `init` y `destroy` están reservados como miembros de clase; los nombres canónicos de operadores (`add`, `sub`, ..., `copy`) todavía no.
- **Mangling**: un método se emite como función local `_r2_m_<mod>_<lenClase><Clase>_<lenMétodo><método>` (por ejemplo `_r2_m_1p_1C_1m`). Las funciones asociadas usan el mismo prefijo. El inicializador usa la forma especial `_r2_o_<mod>_<lenClase><Clase>_init` (por ejemplo `_r2_o_1p_1C_init`) y el destructor `_r2_o_<mod>_<lenClase><Clase>_destroy` (`_r2_o_1p_1C_destroy`). `--dump-symbols` imprime `class X ... size=N align=A`, los campos y una línea `fn X::m ...`/`fn X::init ...`/`fn X::destroy ...`; `--dump-ir` usa los nombres cualificados `X::m`, `X::init` y `X::destroy`.
- **Estado**: las clases todavía no se pueden exportar en módulos: `pub class` / `export class` dan `classes cannot be 'pub' or 'export' yet` y no se escriben en `.r2i` ni en headers. `op`, la destrucción automática al salir del scope y `defer` no están implementados; `destroy`, `drop`, `reset` y el destructor dentro de `delete` sí. Los métodos no-`priv`, incluido `init`/`destroy`, no disparan el warning de no usados; los `priv` sí cuando la clase se usa.

---

## 4. Operadores: precedencia y asociatividad

De mayor a menor precedencia. Todos los binarios son asociativos por la izquierda excepto las asignaciones (`=`, `+=`, …), que lo son por la derecha.

| Prec | Operadores | Resultado |
|------|-----------|-----------|
| postfijo | `expr "[" expr "]"` / `expr "." IDENT` / `expr "(" args ")"` | tipo del elemento (lvalue) / tipo del campo (lvalue) / tipo del primer retorno (llamada, §8.2) |
| unario | `-` `!` `~` `@` `$` (prefijo), `sizeof`, `alignof`, `offsetof` | `-`/`~` preservan tipo, `!` → `i64`, `@e` → tipo apuntado, `$e` → `ptr T`, `sizeof`/`alignof`/`offsetof` → literal flexible (`i64`) |
| cast | `expr as type` (postfijo, asociativo por la izquierda) | el `type` escrito (cualquier numérico, o un puntero si el operando ya es `ptr`) |
| 11 | `*` `/` `%` | tipo de los operandos |
| 10 | `+` `-` | tipo de los operandos |
| 9 | `>>` `<<` | tipo del operando izquierdo |
| 8 | `<` `>` `<=` `>=` | `i64` |
| 7 | `==` `!=` | `i64` |
| 6 | `&` | tipo de los operandos |
| 5 | `^` | tipo de los operandos |
| 4 | `\|` | tipo de los operandos |
| 3 | `&&` | `i64` |
| 2 | `\|\|` | `i64` |
| 1.5 | `cond ? a : b` | tipo unificado de las ramas (ver §5) |
| 1 | `=` `+=` `-=` `*=` `/=` `%=` `&=` `\|=` `^=` `<<=` `>>=` | tipo del LHS |

Los compuestos (`+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`) son tokens propios, no `+` seguido de `=`; `<<=`/`>>=` son tokens de tres caracteres. `x OP= e` es exactamente `x = x OP e` (misma precedencia de asignación y misma asociatividad por la derecha).

El ternario `cond ? a : b` se sitúa entre `||` y la asignación: `x = c ? a : b` es `x = (c ? a : b)`, `c ? a : b = x` es `(c ? a : b) = x` y las condiciones/ramas admiten cualquier expresión de mayor precedencia sin paréntesis. La rama `else` se reanaliza a la precedencia del ternario, así que `a ? b : c ? d : e` es `a ? b : (c ? d : e)` (asociativo por la derecha) y una rama `then` puede contener otro ternario sin paréntesis: `a ? b ? c : d : e` es `a ? (b ? c : d) : e`.

La indexación es postfija y se encadena por la izquierda (`p[i][j]` es `(p[i])[j]`). La llamada `expr "(" args ")"` también es postfija y se encadena igual: `table[i](x)`, `s.cb()`, `(c ? f : g)(x)`, `pick(1)(3, 4)` (§8.2).
El unario tiene la precedencia más alta tras la indexación y su operando es otro unario: `-x + y` es `(-x) + y`, y `- -x` / `@$x` están permitidos.
`sizeof`, `alignof` y `offsetof` van a la misma altura que el unario, pero su operando (`offsetof(T, campo)` es una forma propia con un tipo y una ruta de campos) no es una expresión cualquiera: `sizeof`/`alignof` admiten un tipo o una expresión "cerrada" (una variable, una desreferencia `@p`, una indexación `p[i]`, un campo `p.y` o un literal string), así que `sizeof(i64)`, `sizeof(x)`, `sizeof(@p)`, `sizeof(p[0])`, `alignof(p.y)` y `sizeof("abc")` valen y `sizeof($x)` o `sizeof(1 + 2)` son error.
El cast está justo debajo del unario y por encima de todos los binarios (como en Rust: `unary > as > *`):
`-x as i64` es `(-x) as i64`, `1 as i64 + 2` es `(1 as i64) + 2`, `1 + 2 as u64` es `1 + (2 as u64)`.
Se encadena por la izquierda: `x as i64 as u64` es `(x as i64) as u64`. Los paréntesis fuerzan otro agrupamiento: `(1 + 2) as i64`.
Con punteros: `@p + 1` es `(@p) + 1` (suma al *valor* apuntado, no al puntero) y `$x as ptr i64` es `($x) as ptr i64`.

---

## 5. Expresiones

- Literales enteros, char y string; identificadores; llamadas `f(a, b)`; llamadas **indirectas** `expr(args)` sobre un valor `ptr fn` (§8.2); expresiones entre paréntesis.
- **Acceso a variante `Tipo.Variante`**: `Tipo` debe ser el nombre de un enum y `Variante` una variante existente; la expresión es una constante de compilación del tipo del enum (ver §3.1). El `.` se resuelve dentro del primario, así que `Dias.Lunes as u8` es `(Dias.Lunes) as u8`.
- **Cast `expr as type`** (cualquier tipo numérico, `char` = `i8`): conversión explícita que acepta cualquier operando no-`void` y devuelve exactamente el tipo escrito. Sobre un valor `any` el cast es el **desembalaje** (`a as T`, §3.4) y `v as any` empaqueta explícitamente. Todas las combinaciones están permitidas (incluido el cast identidad `x as i64` con `x : i64`), pero el cast identidad emite un **warning** (`redundant cast to 'i64'; the value already has that type`); un cast que cambia el tipo nunca advierte. Entre 64 bits (`i64<->u64`) es reinterpretación de bits (wrap/módulo 2^64); hacia un tipo narrow es truncado al ancho; hacia 64 bits desde un narrow es extensión con el signo del origen (`i8`→`i64` extiende con signo, `u8`→`u64` con ceros). El cast no es asignable: `(x as i64) = 1` es error (`left side of '=' must be a variable`). Como cualquier expresión, puede aparecer en inits, args, `return` y condiciones.
- **Asignación `=`**: es una expresión. El lado izquierdo debe ser una variable; el derecho debe ser del mismo tipo o ensanchar a él en silencio (ver punto 3; si no, error `cannot assign X to a variable of type Y; use an explicit cast with 'as'`, reportado en el `=`). Devuelve **el valor asignado**, por lo que puede encadenarse (`x = y = 1`) y usarse como expresión, incluso como condición. Asignar una variable a sí misma (`x = x;`) emite un **warning** (`'x' is assigned to itself; this has no effect`).
- **Asignación compuesta `x OP= e`** (`+= -= *= /= %= &= |= ^= <<= >>=`): es una expresión que lee `x` una sola vez, calcula `x OP e` y guarda el resultado en `x`, devolviéndolo; puede encadenarse (`a += b += 1` es `a += (b += 1)`). El LHS debe ser una variable (si no, error `left side of compound assignment must be a variable`, reportado en el operador). Para `+= -= *= /= %= &= |= ^=`, el RHS se adapta al tipo del LHS con las reglas de asignación (mismo tipo, literal flexible o ensanche implícito); si no, error `cannot assign X to a variable of type Y; use an explicit cast with 'as'` en el operador, igual que `x = x OP e`. `<<=` y `>>=` siguen las reglas del desplazamiento binario: el RHS puede ser cualquier numérico y el resultado tiene el tipo del LHS. `x /= 0` y `x %= 0` reportan `division by zero` en el `0`.
- **`&&` y `||`**: short-circuit. Si el resultado queda decidido por el operando izquierdo, el derecho no se evalúa. Un operando constante emite un **warning** (`constant operand of '&&' is always true/false`), también a través de un cast (`(1 as i64) && x` advierte). Un puntero y `NULL` son operandos válidos (y `NULL` advierte); un literal string no.
- **`$x` (dirección de)**: `NODE_ADDR`, unario, sobre una **variable escalar**, una desreferencia o una indexación (`$@p` da el propio `p`; `$p[i]` da `p + i`). Da `ptr T` con el tipo del operando. Sobre un array o slice en crudo es error: el array suelto ya decae a puntero a su primer elemento, usa el array directamente.
- **`@p` (desreferencia)**: `NODE_DEREF`, unario, sobre un `ptr T` no-`void`. Es un **lvalue**: vale como valor (`var y : i64 = @p;`) y como destino (`@p = 1;`, `@p++;`, `@p += 2;`). `@` y `$` son prefijos de la máxima precedencia, así que `@p + 1` es `(@p) + 1` (y `$x as ptr i64` es `($x) as ptr i64`).
- **`p[i]` (indexación)**: `NODE_INDEX`, postfijo, sobre un `ptr T` no-`void` y un índice entero. Es azúcar exacta de `@(p + i)` y produce el mismo *lvalue* (`p[i] = 1;`, `p[i]++;`, `p[i] += 2;`, `var y : i64 = p[i];`, `$p[i]`). Se encadena: `pp[i][j]` es `(pp[i])[j]`. Mismos errores que `@`: `ptr void` (`cannot index 'ptr void'; cast it to a typed pointer first`), no puntero (`cannot index a value of type T`) e índice no entero (`pointer indexing requires an integer index`). El warning de `NULL` conocido también aplica. Un literal string no es indexable (`"abc"[i]` es error). No hay comprobación de límites. Los arrays y slices usan la misma sintaxis postfija (`a[i]`, `m[i][j]`), con el offset escalado y las reglas de §3.3.
- **`new`**: expresión primaria `new [ "(" expr ")" ] type_atom ( { "[" expr "]" } | "(" [ args ] ")" )` (§3.6). Sin allocator usa `malloc` (exige `import "stdlib";`); con `(expr)` usa ese puntero. El resultado es un slice de rango N si hay dimensiones, un `ptr T` si no, y `ptr Clase` en la forma con constructor. El tipo base es un átomo de tipo y el primer `[` es siempre una dimensión de la reserva; para indexar el resultado inmediatamente se escribe `(new ...)[i]`. Si el tipo es una clase, el `(...)` tras el nombre son argumentos de `init` (no un allocator). Es una expresión con efectos (asigna memoria y puede llamar a `init`), así que una `new` usada como sentencia no emite el warning de "expresión sin efecto" (pero pierde la referencia y filtra la reserva).
- **`sizeof`**: acepta un **tipo** (`sizeof(i64)`, `sizeof(ptr i32)`, `sizeof(MiEnum)`, `sizeof(Alias)`, `sizeof(Punto)`) o una **expresión** que sea una variable, una desreferencia, una indexación, un campo o un literal string (`sizeof(x)`, `sizeof(@p)`, `sizeof(p[0])`, `sizeof(p.y)`, `sizeof("abc")`). Cualquier otra cosa (`sizeof(f())`, `sizeof(1 + 2)`, `sizeof(sizeof(x))`) es error (`operand of sizeof must be a variable, dereference, index, string literal or type`), igual que `sizeof(void)`. El resultado es una constante plegada en compilación y **flexible como cualquier literal**: adopta el tipo que pida el contexto si cabe (`var t : i64 = sizeof(i32)`, `var u : u64 = sizeof(i32)`, `var a : i8 = 1 + sizeof(i8)`), así que no hay que castear. Un literal string mide sus bytes más el terminador (`sizeof("abc")` = 4).
- **`alignof`**: acepta los mismos operandos que `sizeof` (un tipo o una variable, desreferencia, indexación, literal string o campo) y se pliega igual a una constante **flexible** `i64`; no llega a IR. `alignof(i32)` = 4, `alignof(ptr i64)` = 8, `alignof('a')` = 1, `alignof("abc")` = 1. De un **array** da la alineación de su elemento hoja (`alignof(i32[4])` = 4, `alignof(u16[3])` = 2, `alignof((ptr i64)[2])` = 8); de un **slice** da siempre **8**, porque el descriptor contiene un puntero (`alignof(u8[])` = `alignof(i32[])` = `alignof(i32[][])` = 8), y lo mismo una variable o un parámetro de tipo slice; de un **struct/unión** da su `align` (`alignof(Pk)` = 1 para un `packed`); de un **bitfield**, el del holder. `alignof(void)` y `alignof(función)` son error, igual que en `sizeof`, y un struct cuyo layout falló da `type 'T' is incomplete`. La alineación reportada es la que usa el layout: `alignof(T)` predice el offset de un campo (ver `offsetof`).
- **`offsetof`**: `offsetof(T, campo)` o `offsetof(T, campo.subcampo...)`, con `T` un struct/unión (también a través de un alias). Se pliega a una constante flexible `i64` y no llega a IR. Un `T` que no sea struct/unión es error (`offsetof requires a struct or union type, not 'i32'`), un campo inexistente `struct 'X' has no field 'y'`, un segmento bitfield `offsetof cannot be applied to bitfield 'x'` y un segmento intermedio que no sea struct/unión `offsetof cannot traverse field 'u' of type u8` (arrays y punteros incluidos; un segmento **final** array o puntero sí vale y devuelve el offset del campo). Un campo de unión está en 0 y una ruta a través de una unión suma 0 y sigue con los campos de la alternativa elegida. Los campos aplanados de structs/uniones anónimos se resuelven como si fueran del padre (`offsetof(Cpu, lo)` = 4) y en `packed` devuelve los offsets reales sin padding.
- **Acceso a campo `expr . IDENT`**: `NODE_DOT` sobre un struct o una unión da un *lvalue* del tipo del campo (ver §3.4). Sobre un `ptr struct` desreferencia automáticamente (`p.campo` ≡ `(*p).campo`); sobre un array/slice de structs el lado izquierdo puede ser una indexación (`arr[i].campo`). Sobre un enum o un tipo cuyo tipo representado sea un enum, el `.` resuelve a una variante (`Dias.Lunes`), una constante de compilación del tipo del enum (ver §3.1). Sobre un array o un slice, solo `.len` es válido. Cualquier otra combinación es error (`struct 'X' has no field 'y'`, `'x' is not an array or a slice`, `'X' is not an enum type`, etc.). El acceso a campo es lvalue: `p.x = 1;`, `p.x += 2;`, `p.x++;`, `var y : i32 = p.x;`, `$p.x`.
- **Ternario `cond ? a : b`**: `NODE_TERNARY`. La condición debe ser de tipo no-`void` (mismas reglas que una condición normal). Las dos ramas se unifican como en un binario: primero decaen arrays/slices y se convierten los literales string a `ptr i8`, un `NULL` adopta el tipo puntero de la otra rama, y después se aplican las reglas de unificación (literal flexible, ensanche implícito); dos tipos nominales distintos o dos punteros no relacionados son error (`type mismatch` / `pointer type mismatch`). Dos valores de la **misma** clase también se unifican (las ramas se construyen y se elige/copia la elegida); otros agregados siguen siendo error. El resultado tiene el tipo unificado y es un valor (no un lvalue). Solo se evalúa la rama elegida: para escalares, a `-O3` se baja a `select`/`cmov` cuando la condición y ambas ramas son puras (sin llamadas, asignaciones, `++`/`--`), y en cualquier otro caso a un salto con dos `MOVE`; las ramas de clase se bajan siempre a un salto que construye/copia en un temporal. Los efectos laterales ocurren exactamente una vez y en orden de fuente. Ejemplos: `var x : i64 = c ? 1 : 2;`, `var p : ptr i64 = c ? a : NULL;`, `f(c ? x : y);`.
- **Aritmética y comparación de punteros**: ver §3 (bloque "Punteros"). `p + n`/`p - n` dan `ptr T`, `p - q` da la distancia en elementos (`i64`) y las comparaciones solo valen entre punteros del mismo tipo o contra `NULL`. Los literales string entran aquí como `ptr i8`; restar dos literales string advierte.

---

## 6. Statements

- Declaración de variable: `var ID : type (= expr)? ;`
- Expresión: `expr ;`. Una sentencia de expresión cuyo resultado se descarta y que no tiene efectos — sin llamada, asignación, asignación compuesta ni `++`/`--` en toda la expresión — emite un **warning** (`expression has no effect; its result is discarded`): `x + 1;`, `@p;`, `p[0];`, `x as i64;`.
- **`i++;` / `i--;`**: solo postfijo y solo como sentencia (también vale como update de un `for`). Se representa con nodos propios `NODE_INC`/`NODE_DEC`; el operando debe ser una variable y el IR emite carga, suma/resta de `1` del tipo del operando y store. `++i` es error (`'++' cannot be used as a prefix; use the postfix form as a statement`) y usarlo dentro de una expresión también (`x = i++;`, `return i++;` → `'++' is only valid as a standalone statement on a variable`).
- Bloque: `{ statement* }`
- `if (cond) body { elif (cond) body } [ else body ]` — el cuerpo es cualquier statement, normalmente un bloque.
- `while (cond) body [ else body ]`
- `for (init; cond; updt) body [ else body ]`:
  - `init`: declaración `var` (con su `;`), expresión, o vacía.
  - `cond`: expresión o vacía (vacía = verdadero: `for(;;)` es un bucle infinito válido).
  - `updt`: una o varias expresiones separadas por comas (ej. `i++, j--`), o vacía.
- **`break;` / `continue;` / `break N;` / `continue N;`** — solo dentro de un `while` o un `for`; fuera de un bucle es error (`'break' is only valid inside a loop`, `'continue' is only valid inside a loop`). `N` es siempre un **literal entero** (por defecto `1`) y cuenta los bucles desde el más interno: `break 2` sale de dos bucles y `continue 2` abandona el bucle interno y continúa el siguiente (en un `for`, por su update). `N = 0` es error (`'break 0' is invalid; the loop level must be at least 1`) y un nivel mayor que el anidamiento también (`'break 2' exceeds the 1 enclosing loop`). En un `for`, `continue` ejecuta el **update** antes de volver a la condición. El `for` por índice se desazucara a bucles anidados, así que cada nivel sintético cuenta como un bucle real: `break` dentro de `for (i, j : m)` rompe solo el bucle de `j` y hay que usar `break 2` para salir de la matriz. Un `break` que sale de un bucle invalida la garantía de retorno de `while (1)`/`for (;;)` (§8), y el código que sigue a un salto en el mismo bloque avisa (`code will never be executed`).
- **`else` en bucles**: `while (cond) body else body` y `for (...) body else body`. Se ejecuta **solo** cuando el bucle termina de forma natural, es decir, cuando su condición se vuelve falsa. Un `break` —también `break N` que salga de este bucle— y un `continue N` (N > 1) que lo abandone lo **saltan**; un `continue` normal del propio bucle no impide que el `else` corra más tarde. En un `for`, el `else` ve las variables declaradas en el init (su scope abarca todo el statement). En un `for` por índice, el `else` pertenece al bucle exterior (el recorrido completo), no a cada dimensión. El `else` está léxicamente **fuera** del bucle: un `break`/`continue` escrito dentro de él apunta a los bucles que lo envuelven (error si no hay ninguno). Si el `else` siempre retorna y ningún `break` sale del bucle, el bucle garantiza retorno para el análisis de §8.
- `for (var idx : T : arr) body [ else body ]`, `for (var idx : arr) body [ else body ]` y `for (idx : arr) body [ else body ]` — recorren un array o un slice (de rango 1 o N) con **un índice por dimensión**, y `idx` es el **índice**, no el elemento. Con varios índices (`for (var i : i64, j : i64 : mat)`, `for (var i, j : mat)`, `for (i, j : mat)`) el parser los desazucara a bucles **anidados**, uno por índice, con el AST idéntico al escrito a mano y conservando los spans originales: el nivel k recorre `arr[idx0]...[idxk-1]` con la cota `arr[idx0]...[idxk-1].len`, o sea `for (var i : i64 = 0; i < mat.len; i++) for (var j : i64 = 0; j < mat[i].len; j++) body` (`idx = 0` cuando reutiliza una variable y, sin tipo, el índice es `i64`; el **último** índice de la forma con tipos puede omitir el suyo). Se admiten hasta 8 índices (`too many index variables in the 'for' (max 8)`). `arr` es cualquier expresión con `.len` (un nombre, `m[i]`, ...). Con menos índices que dimensiones se recorren las primeras (una matriz con un solo índice itera filas); con más, el error sale del `.len` de más. Si una longitud constante no cabe en el tipo de su índice, salta el error del literal plegado (`literal 300 does not fit in type i8`); en un slice la longitud es runtime y no se comprueba. El índice de cada dimensión debe ser un objetivo asignable: un identificador, una desreferencia `@p` o un índice `a[k]` (`for (1 : a)` y `for (a + 1 : a)` dan un único error, nunca un crash).
- **`switch (expr)`**: `switch (expr) { case (v1, v2) { ... } ... default { ... } }`. `expr` debe ser numérico o un enum (también a través de un alias o newtype: se mira la representación) y se evalúa **una sola vez**. Cada `case` lleva entre paréntesis una lista, separada por comas y con al menos un valor, de **expresiones constantes** (`case value must be a constant expression`) que se convierten al tipo del switch con las reglas de asignación (literal flexible o cast explícito; si no, `case value of type X does not match the switch type Y; use an explicit cast with 'as'`). Los valores duplicados, también entre cláusulas distintas, son error (`duplicate case value`). No hay fallthrough: al terminar el cuerpo de una cláusula se sigue después del `switch`. Todo `switch` necesita un `default` (`switch requires a 'default' clause`), que puede ir en cualquier posición; un segundo `default` es error (`duplicate default clause`). El cuerpo de una cláusula puede ser un bloque o una sola sentencia (se envuelve en un bloque implícito), y las variables declaradas dentro no escapan. Ejemplo: `switch (d) { case (Dias.Lunes, Dias.Martes) { return 1; } default { return 0; } }`.
- **`delete [ "(" expr ")" ] expr ;`** — libera el puntero (o el puntero de datos de un slice) con `free`, o con el deallocator `expr` cuando se escribe `delete(expr) x;`. El operando debe ser un `ptr T`, un slice o `NULL`; las formas sin paréntesis exigen `import "stdlib";` y las reglas completas (firma del deallocator, allocator propio, globals y `static`) están en §3.6. Si el tipo es una clase con destructor, `destroy` corre antes de liberar. El operando no se modifica ni se pone a `NULL`: sigue conteniendo la dirección liberada.
- **`drop expr;` / `reset target(args);`** — destrucción y reinicialización explícitas de una clase (§3.7). `drop` llama solo a `destroy` (sin liberar); `reset` exige destructor e inicializador y hace `destroy` + `init` sobre el mismo lvalue. `drop`/`reset` sobre parámetros tienen reglas de préstamo: `drop`/`delete` no valen sobre parámetros (incluido `self`), pero `reset` sí.
- **`static var` / `static arr`**: declaración de duración estática **dentro de una función** (a nivel top-level es error: `static variables can only be declared inside a function`). `static var x : T [= expr];` y `static arr a : T[N] [= expr];` reservan **una sola** instancia (`static var` con tipo array es error: `a 'static' view is not supported; use 'static arr'`; las vistas son locales) para toda la ejecución, aunque la declaración esté en un bloque o en el cuerpo de un `case`; el nombre sigue las reglas de scope normales (shadowing incluido). El inicializador, si existe, debe ser una **expresión constante**: literal, `-literal`, `~literal`, literal string, `NULL`, `$global`, o un inicializador de array o struct recursivamente hecho de esos, designado y parcial incluido (`static var q : P = {.y = 2};` y `static var r : P = {};` valen; `a 'static' initializer must be a constant expression` si no). La excepción son `new` y los constructores de clase `Clase(args)` (§3.6/§3.7): se ejecutan una vez desde `__r2_init`, no pueden referenciar variables locales (`a 'static' initializer with 'new' cannot reference locals`) y el `static` con dimensiones dinámicas funciona como un global. La inicialización ocurre una sola vez, **antes de los inicializadores dinámicos de globales** (así un global init que llame a una función que lea un `static` ve el valor ya inicializado), y el almacenamiento es un global interno (`sv.<k>.<nombre>`); sin inicializador empieza a cero. Ejemplo: `static var calls : i64 = 0; calls += 1;`.
- `return expr? ;` — en una función `void`: `return;` (devolver un valor es error: `function returning void cannot return a value`). En una función `T`: `return expr;` con tipo `T`, un array/slice compatible con `T` o algo que ensanche a `T` en silencio; si no, error (`cannot return X from a function returning Y; use an explicit cast with 'as'`); sin valor, error (`missing return value of type T`). En una función de varios retornos, `return a, b;` devuelve exactamente tantos valores como tipos declarados (§8).
- **`a, b = f();`** — multi-asignación. El lado derecho debe ser una llamada a una función con varios retornos (`the right side of a multi-assignment must be a call to a function that returns several values`) y el número de destinos no puede superar el de valores (`this function returns N values, but M are assigned`); con menos destinos se descartan los sobrantes (`a = f();` se queda con el primero). Cada destino es un lvalue cualquiera — `a`, `p.x`, `v[0]`, `@p`, un global — y se valida como el lado izquierdo de `=` (un destino de tipo array es error). La sentencia se reconoce por la coma: el primer destino se analiza como expresión y, si lo que sigue es una `,`, el resto de destinos se analizan con precedencia 2 para que el `=` no se consuma dentro del segundo.
- **`var a : i64, b : u32 = f();`** — la forma declarativa de la multi-asignación: el parser la reconoce cuando hay dos o más declaraciones, **ninguna salvo la última** lleva inicializador y la última sí lo lleva; entonces construye la misma sentencia y sema declara los destinos ahí mismo (`assigned` desde el principio). Consecuencia: `var a : i64, b : i64 = 3;` (varios destinos con un inicializador que no es una llamada multi) pasa a ser error, mientras `var a : i64 = 1, b : i64 = 2;` y `var a : i64, b : i64;` siguen siendo declaraciones normales en cadena. `static var a, b = f();` es error de parser (`a 'static' declaration cannot initialize several variables`).
- Una llamada multi-retorno usada en cualquier otro contexto vale **su primer valor**: `a = f();`, `var a : i64 = f();`, `f(g())`, `x + g()`, `if (g() > 6)`, `return g();`. Como sentencia descarta todos los valores.

---

## 7. Scopes y variables

- Cada bloque `{}` abre un scope. Los cuerpos de `if`/`while`/`for` sin bloque abren un scope propio.
- Redeclarar un nombre **en el mismo scope** es error. Shadowing de scopes exteriores está permitido.
- Una `static` sigue las reglas de scope del nombre (shadowing incluido) pero su **almacenamiento es único por declaración** y se inicializa una sola vez, antes de los inicializadores dinámicos de globales (ver §6).
- Una local no puede referenciarse antes de su declaración (el init se chequea antes de declarar la variable).
- **Local sin init**: permitida. Si se lee sin haberse asignado, sema emite un **warning** (no error). Su valor en runtime es **indefinido** (lo que haya en el stack).
- **Local `const`**: un tipo const exige inicializador (§3.5) y el lvalue es de solo lectura: `var x : const i64;` es error y `x = ...`, `x += ...` y `x++` se rechazan (`cannot modify const variable 'x'`). Un `static var y : const i64 = ...` sigue las mismas reglas.
- El análisis de "asignada" es conservador y sensible al flujo: una variable cuenta como asignada si tiene init, si es parámetro, si se le asignó antes en el mismo flujo incondicional, o si **todas** las ramas de un `if`/`elif`/`else` la asignan (un `if` sin `else` no cuenta; lo asignado dentro de un `while`/`for` tampoco escapa del cuerpo). En otro caso la lectura emite el warning (`'x' is used without being assigned`). El cuerpo de un `if` con condición siempre-true se trata como incondicional.
- **Local no leída**: una local que nunca se lee emite un **warning** (`unused variable 'x'`) al cerrar su scope, salvo que su nombre empiece por `_`. Escribirla no cuenta como leerla: `var x : i64 = 1; x = 2;` también advierte. Leerla en cualquier expresión (incluidas una condición, un `return`, `$x`, `@p`, el destino de una asignación compuesta o un `++`/`--`) cuenta. La advertencia se reporta en el nombre declarado.
- **Valor asignado no usado**: si a una local o parámetro se le asigna un valor que una escritura posterior del mismo flujo sobrescribe sin que se haya leído antes, sema emite un **warning** (`value assigned to 'x' is not used`) en el punto del init o de la asignación: `var x : i32 = 7; x = 6; return x;` advierte sobre el `7`. Una lectura de la variable anula el seguimiento pendiente; entrar en una región condicional (cuerpo de un `if` con condición no constante, `elif`/`else`, cuerpo de `while`/`for`, RHS de `&&`/`||`) o volver de una bifurcación lo descarta, de modo que el análisis solo puede omitir avisos, nunca inventarlos. No se aplica a globales ni a variables cuya dirección se toma (`$x`). Si la variable nunca se lee, se emite solo el warning de local no leída.
- **`NULL` conocido**: análisis conservador y sensible al flujo, análogo al de "asignada": un puntero es conocido `NULL` si su init o su última asignación incondicional fue `NULL` y todos los caminos vivos llegan con `NULL` (un `if`/`elif`/`else` lo conserva solo si **todas** las ramas asignan `NULL`; un `if` sin `else`, un `while`/`for` o el RHS de `&&`/`||` no lo conservan). Cualquier otra escritura (incluida una copia `p = q`) lo pierde. Solo alimenta el warning de desreferencia nula (§3); nunca es un error.

---

## 8. Funciones

- Solo top-level. Declarar una función dentro de otra es error. El nombre de una función es también un **valor** de tipo `ptr fn(...)` (§8.2).
- `fn nombre(params)? (: ret_type)? block` — sin `: tipo` el retorno es `void`.
- **Varios retornos**: `fn nombre(params) : T1, T2, ... block`. El primer tipo es el del nodo de retorno y los demás se encadenan como tipos adicionales. Cada uno se comprueba igual que un retorno único (no valen `void` en la lista: `void cannot be used as one of several return types`; un array concreto `T[N]` sí, y devuelve solo el puntero). `return a, b;` debe dar **exactamente** tantos valores como tipos declarados: `this function returns N values, but M were returned` (en singular si `N == 1`), y cada pareja se convierte con las reglas de retorno. En una función de un solo valor, `return a, b;` es ese mismo error. `--dump-symbols` imprime la lista completa (`fn f ... type=i64, u32`).
- **Retorno de slices y arrays**: `: T[]` / `: T[R][C][]` devuelve un **descriptor** igual que el de un parámetro slice, construido desde el valor devuelto y comprobando las dimensiones definidas como en un argumento (mismo rango, mismo elemento hoja y el mismo tamaño en cada dimensión concreta); `: T[N]` / `: T[R][C]` devuelve **solo el puntero** al primer elemento (como un parámetro `T[N]`), nunca una copia del array. Devolver almacenamiento local avisa: `returning a slice of the local 'x'; it dangles when the function returns` (o `returning a pointer to the local 'x'` si el tipo de retorno no es un slice). Se detectan el array local, sus sub-vistas (`m[0]`), el campo array de un struct local, `$local` y los slices locales que provienen de un array local (`var s : i64[] = local_arr; return s;`, propagando por más copias); el análisis es conservador (mira las ramas y solo avisa si el taint sobrevive en todas) y no cruza llamadas, globales, punteros ni campos slice.
- Parámetros: `ID : type [= expr]`, por valor. Siempre se consideran asignados. Un parámetro que nunca se lee emite un **warning** (`unused parameter 'a'`), con la misma regla que las locales (§7), salvo que empiece por `_`.
- **Referencias (`&T`)**: solo como tipo de un parámetro (`fn inc(x : &i64)`); `&` en cualquier otra posición es `references are only allowed as function parameter types` (también en locales y dentro de otro tipo, `ptr &i64`). El llamador pasa un **lvalue** directamente (`f(v)`, `f(s.f)`, `f(a[i])`, `f(@p)`), nunca un `ptr T` ni un literal (`argument for reference parameter 'x' must be a variable`), y el tipo del referente debe coincidir exactamente (`argument type ... does not match reference parameter ...`). Dentro el parámetro se usa como una variable normal: leer o escribir desreferencia automáticamente (`x += 1`, `x = 5`, `p.x`), `$x` da la dirección del referente (`ptr T`) y `@x` es error. `sizeof`/`alignof` describen `T`. No hay *rebinding*: `x = ...` siempre escribe el referente. Un parámetro `&` puede pasarse a otro parámetro `&` sin `$` (se reutiliza el puntero), y a un parámetro normal se le pasa el valor (auto-`@`). El referente puede ser cualquier tipo salvo `void`, arrays y slices (`references to arrays and slices are not supported`), no admite valor por defecto (`a reference parameter cannot have a default value`) y en `extern` se traduce al puntero C `T *`. Sema lo baja a `ptr T`, así que la IR, el optimizador y el backend no cambian.
- **Argumentos opcionales**: `fn foo (a : i64, b : i64 = 33) : void` declara `b` con valor por defecto; al llamar `foo(3)` el compilador expande en el **caller** a `foo(3, 33)`. El default debe ser una **constante** evaluable en el caller (literal, `-literal`, string, `NULL`, variante de enum, `$global`, `sizeof`/`alignof`/`offsetof`, o un agregado `{...} as T` de esos; una expresión aritmética de literales también vale porque sema la pliega), y se adapta al tipo del parámetro como un argumento normal (`default argument type X does not match parameter type Y; ...`). Los opcionales van **siempre al final** (`required parameter 'y' cannot follow an optional parameter`); puede haber tantos como se quiera. No se permiten defaults de tipo array o slice (`an array or slice parameter cannot have a default value`). El `.r2i` de módulos (§9.2) guarda los defaults para que el importador pueda hacer la misma expansión en el caller.
- **Parámetros variádicos**: `fn foo (fmt : ptr char, rest : ...i64) body` declara `rest` como variádico. Debe ser el **último** parámetro (`the variadic parameter must be the last parameter`), el nombre es obligatorio (`a typed variadic parameter must be named ('name : ...Type')`) y el tipo también (`rest : ...T`). Dentro de la función `rest` es un **slice** `T[]`: valen `rest.len`, `rest[i]`, `for (var i : rest)`, `sizeof` y pasarlo a cualquier parámetro `T[]`. La llamada acepta **cero o más** argumentos de ese tipo: cada extra se comprueba y convierte a `T` como un argumento normal (`variadic argument type X does not match the element type Y; use an explicit cast with 'as'`), se empaqueta en un array temporal del llamador y se pasa como descriptor (puntero + longitud); sin extras se pasa `NULL` y longitud 0. `T` puede ser cualquier tipo con tamaño —enteros, enums, punteros, structs o arrays concretos como `...i64[2]`—, pero no un slice ni un tipo mixto (`a variadic parameter cannot have a slice element type`) ni una referencia (`...&T` → `a variadic parameter cannot be a reference`). Los parámetros fijos anteriores se comprueban como siempre y los extras van **posicionalmente**: una llamada variádica no admite argumentos con nombre (`a variadic call cannot use named arguments`). Una función variádica **no admite defaults** en ningún parámetro (`a variadic function cannot have default arguments`). **Spread** `f(s...)`: un slice del tipo **exacto** del variádico se pasa directamente (se reutilizan su puntero y su longitud, sin empaquetar ni copiar), escribiendo `...` tras la expresión. Un **array** del mismo rango y elemento que el slice del variádico también se expande (`add(x...)` con `x : i32[5]`), con la misma ruta que su decay: se pasa la dirección del array y una longitud **constante** (la dimensión). Debe ser el **último** argumento (`a spread argument must be the last argument`), no puede ir con nombre (`a spread argument cannot be named`), no puede combinarse con otros extras del variádico (`a spread argument cannot be combined with other variadic arguments`), no puede ocupar un parámetro fijo (`a spread argument cannot be passed to a fixed parameter`), solo existe en una llamada variádica tipada (`spread arguments ('...') are only allowed in a typed variadic call`) y su tipo debe coincidir con el slice del variádico (`spread argument type X does not match the variadic type Y`); así `sum(v...)`, `sum(make()...)` y `add(x...)` reenvían/expanden sin bucle. **`rest : ...any`** declara un variádico de elemento `any` (§3.4), que en una sola llamada acepta valores de tipos **distintos** (`total(1, n, "hi", NULL)`); dentro `rest` es un `any[]` normal (`.len`, `rest[i]`, index-`for`) y cada elemento se desembala con `rest[i] as T`; el spread también vale (`total(v...)` con `v : any[]`). Esta forma tipada es exclusiva de las `fn` normales: `extern` y `export` la rechazan (`typed variadic parameters ('name : ...T') are only allowed in non-extern functions`, `export functions cannot be variadic`); el `...` **sin tipo** de `extern` es otra cosa (§8.1).
- Forward references y recursión permitidas: las funciones se declaran todas antes de chequear los cuerpos.
- Las llamadas comprueban arity (contando los defaults) y, parámetro a parámetro, tipo `T` o ensanche silencioso a `T` (los literales adoptan el tipo del parámetro si caben); si no, error (`argument type X does not match parameter type Y; use an explicit cast with 'as'`). Los argumentos pueden ser **posicionales** o **con nombre**: `foo(3, b = 10)` elige `b` por nombre y los nombrados pueden ir en cualquier orden (`foo(3, c = 4, b = 2)`, `foo(3, c = 3)` omitiendo el del centro). Un argumento con nombre no puede repetirse (`argument for parameter 'b' is given more than once`), debe existir (`unknown parameter 'b' in call to 'foo'`) y no puede haber argumentos posicionales después de uno con nombre (`positional argument cannot follow a named argument`). Los **opcionales** que se pasen deben ir **todos con nombre o todos posicionales** (`foo(3, 10, c = 20)` es error: `optional argument for parameter 'b' must be named; name all optional arguments or pass them all positionally`); los requeridos pueden seguir posicionales aunque haya nombrados, y también se pueden nombrar (`foo(a = 3, b = 10)`).
- **Return obligatorio en todos los caminos** para funciones no-`void` (chequeo conservador):
  - Un `return` garantiza.
  - Un bloque garantiza si alguno de sus statements garantiza.
  - Un `if` garantiza solo si tiene `else` final y todas las ramas (then/elif/else) garantizan.
  - `while`/`for` garantizan si **ningún `break` sale de ese bucle** y, además, (a) su condición es constante verdadera (`while (1)`, `for (;;)`) o (b) su `else` garantiza. Sin break, un bucle con condición no constante solo puede salir por el `else`. Si un `break` sale del bucle, nunca garantizan.

### 8.1 `extern` (interoperabilidad con C)

- `extern fn nombre(params)? (: ret_type)? ;` declara una función definida fuera del programa (típicamente libc), y `extern var nombre : type ;` una variable global definida fuera. Ambas son solo top-level. Una `extern fn` puede declararse **variádica** terminando la lista con `, ...` (`extern fn printf (fmt : ptr i8, ...) : i32;`).
- Nunca llevan cuerpo ni inicializador: `extern var x : i64 = 1;` es error de parser (`extern variables cannot have an initializer`).
- sema comprueba arity y tipos en cada llamada igual que con una función propia (salvo los argumentos extra de una variádica, ver abajo), pero no exige cuerpo. Una `extern var` cuenta como siempre asignada (no dispara el warning de lectura sin asignar).
- **Variádicas sin tipo**: `...` solo puede ser el **último** elemento de la lista, tras una coma (`extern fn printf (fmt : ptr i8, ...) : i32;`) o como único parámetro (`extern fn trace (...) : void;`) y no lleva tipo ni nombre. Solo vale en `extern`: en una `fn` (y en `export fn`) es error (`variadic parameters ('...') are only allowed in extern functions`). Es distinta de la variádica **tipada** `rest : ...T` de las funciones normales (§8); una `extern` no puede declararla (`typed variadic parameters ('name : ...T') are only allowed in non-extern functions`). En una llamada a una variádica los parámetros declarados se siguen comprobando como siempre (tipo, `&T`, defaults y la obligación de pasar los requeridos), pero los argumentos **extra no se comprueban**: se acepta cualquier número, de cualquier tipo, y se pasan en orden tras los declarados (cada uno ocupa un slot de 8 bytes de la ABI entera). No se admiten argumentos con nombre en una llamada variádica (`a variadic call cannot use named arguments`). Los enteros menores de 64 bits se pasan extendidos (con signo o sin él según el tipo), así que `%d`/`%u`/`%c` de `printf` reciben el valor correcto.
- El nombre no se mangla ni lleva prefijo: el assembly emite `call puts` / `movl optind(%rip), %eax`, y la resolución queda para el `gcc` final. La dirección `$optind` también usa el símbolo plano.
- Antes de cada llamada a una función `extern` el backend emite `xorl %eax, %eax` (ABI SysV: `%al` = número de registros vectoriales para funciones variádicas; el compilador lo emite siempre, con o sin variádicos).
- No puede llamarse `main`: `'main' cannot be declared extern` (`extern fn`) y `'main' must be a function` (`extern var`). Los nombres con prefijo `__r2_` o `_r2_` siguen reservados.
- `extern` no cambia la representación: los parámetros y el retorno siguen la ABI entera SysV AMD64 (hasta 6 argumentos en registros, el resto en la pila) y los punteros/`ptr void` se pasan como punteros C.

---

### 8.2 Punteros a función

`ptr fn(a : i32, b : i32) : i32` es **un puntero** (§3) a una firma de función; `fn(...)` sin `ptr` es un alias del mismo tipo. La firma lleva tipos, no nombres (los nombres de los parámetros son opcionales: `fn(i32, i32) : i32`), los parámetros `&T` se guardan ya bajados a `ptr T` (a través del puntero se pasa `$x`, como en cualquier `ptr T`) y `void` no vale como tipo de parámetro ni de retorno (error de parser: `expected type`). Los varios retornos se escriben entre paréntesis **en el tipo**: `ptr fn(a : i32) : (i32, u32)`; el `void` de un único retorno se puede omitir, así que `fn(i32)` y `fn(i32) : void` son el mismo tipo. El `...` final marca un variádico estilo C (`ptr fn(fmt : ptr i8, ...) : i32`). Dos firmas son el mismo tipo si coinciden sus parámetros, sus retornos y el `...`; como todo `Type` se interna estructuralmente, dos escrituras equivalentes son intercambiables sin cast. El `const` **externo** de un parámetro o del retorno se ignora al construir la firma (§3.5): `fn f(x : const i32) : const i32` encaja con `ptr fn(i32) : i32`; el const **anidado** (`ptr const i32`) sí forma parte del tipo. Un parámetro puede ser a su vez un puntero a función (`ptr fn(cmp : ptr fn(a : i64, b : i64) : i32) : void`).

Son **valores** las funciones de firma fija —`fn`, `extern fn` y `export fn`—: el nombre de la función, usado como expresión, tiene tipo `ptr fn(...)` y vale en init, asignación, argumento, `return`, elemento de array, campo de struct y global. No son valores las que tienen **valores por defecto** ni las **variádicas tipadas** de R2 (`'f' cannot be used as a value; it has optional or variadic parameters`); un `extern fn` variádico de C **sí** encaja con un tipo `ptr fn(..., ...)` y se llama con argumentos extra. `$f` sigue siendo error (`'f' is a function; use it directly as a pointer value`): el nombre suelto ya es el puntero.

La firma se comprueba al usar el valor: init (`can't initialize 'p' (...) with a value of type ...; use an explicit cast with 'as'`), asignación (`cannot assign ... to a variable of type ...`), argumento (`argument type ... does not match parameter type ...`), `return` (`cannot return ... from a function returning ...`) y campo de un literal. Dos firmas **distintas** solo se convierten con un cast explícito entre punteros (`q as ptr fn(a : i32) : i32`), que es un cast de puntero normal y **no** comprueba la firma: úsalo a sabiendas.

Una llamada indirecta es cualquier expresión postfija seguida de `(`: `callback(4, 5)`, `s.cb()`, `table[i](x)`, `(c ? f : g)(x)`, `pick(1)(3, 4)`. Un nombre que resuelve a una `var` o a un parámetro de tipo `ptr fn` se llama igual que una función (véase el ejemplo al final de esta sección). Las llamadas indirectas:

- son **posicionales** y de **aridad exacta** (`wrong number of arguments in indirect call: got N, expected M`): no admiten argumentos con nombre (`an indirect call cannot use named arguments`), ni defaults, ni spread (`a spread argument ('...') is not supported in an indirect call`);
- comprueban cada argumento contra la firma como una llamada normal (mismo tipo, literal flexible, ensanche silencioso, `ptr fn`↔`ptr void`, arrays/slices/structs y `&T` → `ptr T`);
- en un tipo variádico de C aceptan argumentos **extra sin comprobar**, pasados en orden tras los declarados (como `printf`);
- con varios retornos valen como llamada multi (`a, b = cb();`, `var a, b = cb();`) y en un contexto de un solo valor dan el primero;
- **nunca** son tail calls (no se convierten en `jmp`).

Operaciones permitidas: `==`/`!=` entre el mismo tipo `ptr fn` o contra `NULL` (resultado `i64`), y las cuatro formas de valor de verdad (`!p`, `p && q`, `p || q`, `if (p)`). Todo lo demás tiene diagnóstico propio: `<`/`>`/`<=`/`>=` (`comparison of function pointers is not allowed; only '==' and '!=' are supported`), `+`/`-` y `+=`/`-=` (`pointer arithmetic on a function pointer is not allowed`), `@p` (`cannot dereference a function pointer; call it directly`) y `p[i]` (`cannot index a function pointer; call it directly`). `ptr fn` ↔ `ptr void` es implícito en las dos direcciones como cualquier puntero (para pasar un callback por una API C de `void *`), `$p` da `ptr ptr fn`, y `sizeof`/`alignof` de un `ptr fn` son 8 (una firma suelta no se puede medir: es `ptr fn` o nada).

Se pueden declarar en locales, parámetros, retornos, `static`, globales, campos de struct/unión y elementos de array (un array de punteros a función se escribe con paréntesis: `(ptr fn(a : i32) : i32)[4]`):

```r2
type Op = ptr fn(a : i32, b : i32) : i32;

fn suma (a : i32, b : i32) : i32 { return a + b; }
fn resta (a : i32, b : i32) : i32 { return a - b; }

fn pick (flag : i32) : Op { return flag ? suma : resta; }

struct Handlers { op : Op; count : i32; }

var global_cb : Op = suma;              // global con inicializador constante

pub fn main () : i64
{
    var callback : fn(a : i32, b : i32) : i32 = suma;   // `fn` == `ptr fn`
    arr table : Op[2] = {suma, resta};
    var h : Handlers = {.op = resta, .count = 1};
    var raw : ptr void = table[1];
    var back : Op = raw;                // ptr void -> ptr fn, implícito
    var a : i32 = callback(4, 5);       // 9
    var b : i32 = table[0](1, 2);       // 3
    var c : i32 = h.op(9, 4);           // 5
    var d : i32 = pick(1)(7, 8);        // 15
    var e : i32 = global_cb(2, 2);      // 4
    if (back == resta) a += 1;
    if (back != NULL) a += 1;
    return a as i64 + b as i64 + c as i64 + d as i64 + e as i64;
}
```

Un `static var cb : Op = suma;` guarda el puntero en el almacenamiento estático (`sv.<k>.cb`) y lo inicializa una sola vez, antes de los inits dinámicos de globales, igual que cualquier otro `static`. Un inicializador global de tipo `ptr fn` sigue la regla de las globales puntero (§9): es una **constante de enlace** si es `NULL`, `$g` o el **nombre de una función**; en `-O2`/`-O3` se liga en `.data` como `.quad <símbolo de la función>` y `__r2_init` deja de escribirlo. Un campo `ptr fn` de un struct global no se liga (los punteros no forman imágenes de bytes), así que se escribe desde `__r2_init`.

---

## 9. Globals

- `var ID : type (= expr)? ;` a nivel top-level. Cualquier otra cosa a nivel top-level es error: `only variable, function, enum and type declarations are allowed at the top level`.
- Todas se declaran antes de chequear los inits: las **forward references están permitidas**.
- Referencias a variables globales futuras permitido, ej: `var x:i64 = y; var y:i64 = 73` es válido
- Una global **sin init vale 0** en runtime (vive en `.bss`), pero su lectura **sí advierte** igual que una local sin asignar: solo la evitan un init o una asignación previa en el flujo (una asignación en una función chequeada antes también la silencia, en orden fuente).
- **Auto-referencia** (`var x : i64 = x;`): error.
- **Cadenas circulares** entre inits (`var y : i64 = x; var x : i64 = y;`): error. Al chequear el init de una global, si el init referencia a otra global cuyo init aún no se chequeó, ese init se chequea en ese momento (recursivamente); referenciar una global cuyo init está a medio chequear es el ciclo, y se reporta en el ref que lo cierra (un error por ciclo).
- Solo se siguen **referencias directas** a globals en los inits. Los ciclos a través de llamadas a funciones dentro de inits no se detectan.
- Las referencias a través de casts cuentan igual que las directas: `var y : u64 = x as u64;` fuerza el init de `x` antes que el de `y`, y `var x : i64 = y as i64; var y : i64 = x as i64;` es circular. Lo mismo para escrituras (`var y : i64 = (x = 1 as i64);` cuenta como referencia a `x`).
- Los inits pueden llamar funciones; esas llamadas se ejecutan en orden fuente durante el arranque.
- El init de una global de tipo puntero debe ser una **constante de enlace**: `NULL` (o un cast de `NULL`), un literal string (a `ptr i8`), la dirección de una global (`$g`) o, para un `ptr fn`, el **nombre de una función** (`var cb : Op = suma;`). Un valor es error: `var p : ptr i64 = q;` (`global pointer initializers only support NULL, a string literal or the address of a global`); `$@p` tampoco vale, porque depende de un valor. En `-O2`/`-O3` estos inits se ligan estáticamente en `.data`; en `-O0`/`-O1` los escribe `__r2_init` en runtime.

### 9.1 Visibilidad (`pub` / `export`) y nombre de módulo

- Por defecto cada `fn`/`var`/`arr` top-level es **privada del fichero**: su símbolo se mangla (`_r2_f_<mod>_<len><nombre>`, `_r2_v_...`, `_r2_sv_...`) y el assembly lo marca `.local`.
- `pub fn`, `pub var`, `pub arr` lo hacen público; también se acepta `pub` en `enum`, `struct`, `union`/`packed` y `type`/`newtype` (que no emiten símbolo). El símbolo manglado cambia de familia (`_r2_pf_...` / `_r2_pv_...`) y el assembly emite `.globl`. `export` se acepta en los mismos sitios que `pub` salvo `extern`; en tipos no cambia el assembly (no emiten símbolo), pero fija su nombre plano en la cabecera y en el `.r2i`.
- `pub` solo vale en top-level (`'pub' is only valid on top-level declarations`). `pub extern` sí existe: expone una declaración C con su nombre plano en el `.r2i` (una `extern` sin `pub` sigue siendo privada del fichero y no se expone). `pub static` no existe (error de parser: `'static' cannot be 'pub'`).
- `fn main` debe ser `pub fn main() : i64`: `'main' must be declared as 'pub fn main() : i64'`.
- **`export`** es el inverso de `extern`: expone una definición R2 con su **nombre C plano** para llamarla desde C (`export fn callback(...)` → `callback:` + `.globl`; `export var contador` → `contador:` + `.globl`). Implica `pub`, vale para `fn`/`var`/`arr` y para `struct`/`union`/`packed`/`enum`/`type`/`newtype`, y no se combina con `pub` ni con `extern` (`'export' cannot be combined with 'pub'` y viceversa; `expected a declaration after 'export'`). Los símbolos planos siguen protegidos por los prefijos reservados y `main` no se puede exportar (`'main' cannot be exported`). Un nombre exportado que sea keyword de C/C++ es error (`exported name 'int' is a C/C++ keyword; rename it`).
- **ABI de `export`**: como el símbolo es plano, la firma debe ser representable en C. Se rechazan los `export fn` con structs por valor en parámetros o retorno (`export functions cannot take structs by value yet`, `... cannot return structs by value yet`), con slices como parámetro (`export functions cannot take slice parameters`), con retorno array/slice (`export functions cannot return arrays or slices yet`) y con multi-retorno (`export functions cannot return multiple values yet`). Los parámetros `&T` sí valen (se traducen a `T *`).
- **Tipos alcanzables**: toda firma, tipo global o campo alcanzable desde una declaración `export` debe ser a su vez `export`; si no, error (`export declaration exposes non-export type 'T'`). Así la cabecera puede nombrar todos los tipos que aparecen en la API.
- **Arranque desde C**: los inicializadores no constantes se ejecutan en `__r2_init_<mod>`, que el wrapper `main` llama si el módulo define `main`. Un host C que use `export`/`pub` debe llamarla una vez antes de tocar globals con init runtime. Ojo: solo se emite si no está vacía (globals todos constantes o sin globals ⇒ el símbolo no existe; con `-O2` es lo habitual), así que no se puede referenciar de forma incondicional.
- **Cabeceras C/C++ (`-H` / `--header[=DIR]`)**: declaración por declaración, solo lo marcado `export` (fns, vars, arrays y tipos; no `pub` ni `pub extern`). El archivo se llama `<módulo>.h` y va a `DIR` (defecto `include`). Se genera solo si hay declaraciones `export`. Es multiplataforma: include guard `R2_<MOD>_H`, `<stdint.h>`/`<stddef.h>` con tipos de ancho fijo, macro `R2_API` (`__declspec(dllimport)` en Windows salvo `R2_STATIC`/`R2_BUILDING`, `visibility("default")` en GCC/Clang), `extern "C"` para C++, empaquetado con `__attribute__((packed))`/`#pragma pack` y `_Static_assert`/`static_assert` de `sizeof`/`offsetof` para detectar diferencias de layout. Los bitfields se emiten con su tipo contenedor y un aviso de que el layout depende del compilador. Si el módulo tiene init dinámico se declara `R2_API extern void __r2_init_<mod>(void);` bajo `#if R2_HAS_DYNAMIC_INIT`; el valor se fija desde la IR optimizada, igual que el `has_dynamic_init` del `.r2i`. Los enums salen como `enum E : base { E_A, ... };` en C++ y como `typedef base E; enum { E_A, ... };` en C. Los slices se exponen como un descriptor `{ T *data; int64_t len[rango]; }` y los punteros a función como typedefs `r2_fn_N`. **`i8` se escribe `char` en las cabeceras en todo contexto compuesto** donde C lo trata como texto: el pointee de un puntero a cualquier profundidad (`ptr i8` → `char *`, `ptr const i8` → `const char *`), el elemento de un array (`x : i8[N]` como objeto → `char x[N]`, y como parámetro → `char *`), el `data` de un slice de `i8` y `r2_any.tag` → `const char *`. Solo el escalar suelto (tipo de un `var`/global/parámetro/retorno o alias de `i8`) sigue siendo `int8_t`, y `u8` sigue siendo `uint8_t` en todas partes. Así una API C que usa `char *`/`const char *` (rutas y strings) enlaza sin warnings `-Wpointer-sign`. Los tipos con `const` en la cabecera respetan §3.5, incluido `ptr const void` → `const void *`.
- **`module "nombre"`**: `module "nombre";` **renombra** el módulo del fichero y sustituye el nombre derivado de la ruta (§9.1, siguiente punto) en todos sus símbolos manglados y en `__r2_init_<mod>`. Es solo top-level (`'module' can only be declared at the top level`), como máximo una vez (`duplicate 'module' declaration`) y el nombre no puede estar vacío (`module name cannot be empty`). El nombre se codifica como **un único segmento** con la misma longitud prefijada y escaping `$xx` que los segmentos de ruta: `module "math2";` produce el módulo `5math2` (`_r2_pf_5math2_3add`, `__r2_init_5math2`) y `module "m$x";` produce `5m$24x` (el `$` se escapa). Ejemplo: `module "asmmod";` con un `pub fn main` da `_r2_pf_6asmmod_4main`.
- **`depends "lib"`**: declara una librería nativa de la que depende el módulo. Solo vale en top-level (`'depends' can only be declared at the top level`); se admiten varias sentencias y varias cadenas por sentencia (`depends "m", "z"; depends "m";`), deduplicando por orden de aparición. El nombre no puede estar vacío (`library name in 'depends' cannot be empty`) y solo admite `[A-Za-z0-9_+.-]` (`invalid library name '...'; use only [A-Za-z0-9_+.-]`), para poder pasarlo tal cual a `-l` al enlazar. No afecta a la compilación ni al código generado: solo se escribe en la cabecera del `.r2i` y de la `.h` como `// depends: m, z`, de modo que una herramienta externa de gestión de proyectos sepa qué librerías añadir.
- **Nombre de módulo**: sale de la ruta del fuente relativa a la **raíz del proyecto**, que es el ancestro común más profundo entre el directorio actual y el del fuente; si solo comparten `/`, se usa el nombre del fichero. Se resuelven `.`, `..` y los symlinks de **directorio**, pero el **nombre del fichero se conserva tal cual se escribe**: un symlink de fichero `mathlink.r2 -> src/math.r2` compilado desde la raíz del proyecto da el módulo `3src8mathlink`, no `3src4math`. Se quita `.r2`, cada segmento se prefija con su longitud **ya escapada** y los bytes fuera de `[A-Za-z0-9_.]` se escapan como `$xx` (hex minúsculas; `dir with space` → `18dir$20with$20space`). Ejemplos: con cwd `/proyecto` y fuente `src/math.r2` el módulo es `3src4math` (`_r2_pf_3src4math_3add`); desde `/proyecto/build` con `../src/math.r2` sale lo mismo; desde `/` con `/proyecto/src/math.r2` queda `4math`. Un fichero `src/weird-name.r2` produce `_r2_pf_3src12weird$2dname_...`.
- **Prefijos reservados**: a nivel top-level no se puede declarar nada que empiece por `__r2_` ni por `_r2_` (`identifiers starting with '__r2_' are reserved` / `identifiers starting with '_r2_' are reserved`), porque ambos están reservados para símbolos generados. Las locales y los parámetros sí pueden usarlos.

### 9.2 Módulos e importaciones

- Un fichero es un módulo. Se compila de uno en uno y se enlaza después:
  `./build/r2p a.r2 -m modules`, `./build/r2p b.r2 -m modules`, `gcc a.s b.s -o prog`.
  Un normal compile escribe `modules/<módulo>.r2i` (nombre plano; véase §9.1) con la
  interfaz pública; el importador lee ese fichero.
- `import "m";` hace visibles los símbolos `pub`/`export`/`pub extern` de `m` con el
  cualificador `m::`: `m::add(1, 2)`, `m::g`, `m::tabla[0]`, `m::Pair`, `m::E.Two`,
  `$m::g`, `sizeof(m::Pair)`, `offsetof(m::Pair, b)` y
  `var f : ptr fn(i64) : i64 = m::twice;`. El módulo va entre comillas, igual que en
  `module "name"`; una ruta de carpetas se escribe con `/` dentro del string
  (`"std/math"`), con segmentos no vacíos; `::` dentro del string es error. El nombre
  se guarda y se escribe en las cabeceras con `/`; solo los calificadores del código
  siguen usando `::` (`std::math::abs`). `import "m" as x;` cambia el cualificador
  (`x::add`);
  el nombre `m::` ya no existe en ese fichero.
- `from "m" import a;` trae `a` sin cualificar (`a(1, 2)`); `from "m" import a as b, Pair, E;`
  permite alias y tipos. Cargar con `from` no liga el cualificador: para `m::otro` hace
  falta además `import "m";` (se pueden mezclar). No hay comodín (`from "m" import *` es error)
  ni `use`. Con carpetas, `from "std" import math;` carga **solo** `std::math`, no los
  módulos hermanos; si un nombre es a la vez submódulo y símbolo exportado por
  `std.r2i`, gana el submódulo y se emite un aviso.
- Los `import`/`from` son de ámbito de fichero y pueden aparecer en cualquier punto
  top-level (dentro de una función o bloque es error:
  `import declarations are only allowed at the top level`). `import` y `from` son palabras
  reservadas.
- Solo lo declarado `pub` (o `export`/`pub extern`) está en el `.r2i`; un símbolo privado
  no existe para el importador (`module 'm' does not export 'x'`). Un nombre traído por
  `from` no puede repetirse en top-level (error de colisión); una variable o parámetro
  **local** sí puede hacer shadowing. Dos `from` con el mismo nombre local y dos
  cualificadores iguales son error.
- Búsqueda del `.r2i` de `import "m";`: primero el directorio del fuente, luego el de `-m`
  (por defecto `modules`), luego cada `-p DIR` en orden. Si no aparece:
  `cannot find module 'm' (.r2i not found in the source directory, the modules directory
  or a -p path)`. Los `.r2i` son caché de build: si cambia un módulo, recompílalo antes
  que a sus importadores (no hay seguimiento automático).
- **Carpetas de módulos**: si en vez de un `.r2i` hay un directorio con ese nombre en
  alguno de los search paths, se importan **todos** los `.r2i` que contiene,
  recursivamente. Cada módulo queda bajo un cualificador con la ruta completa: con
  `std/math.r2i`, `std/time.r2i` y `std/net/http.r2i`, `import "std";` permite
  `std::math::abs(x)`, `std::time::now()` y `std::net::http::get()`. Un directorio
  sin ningún `.r2i` (ni en subcarpetas) no cuenta como módulo. `import "std" as s;`
  cambia el prefijo (`s::math::abs`), `import "std/math";` importa solo ese submódulo
  (mantiene `std::math::abs`), `from "std" import math;` liga `math::abs` y
  `from "std/math" import abs;` liga `abs` directamente. Las rutas también valen en
  tipos (`std::base::Point`) y en la cabecera `// imports:` de un `.r2i`.
- Un compile normal escribe el `.r2i` **espejando las carpetas del fuente** respecto
  al directorio de trabajo (`std/math.r2` → `modules/std/math.r2i`, y el `.h` en
  `include/std/math.h`); con `module "nombre"` el archivo sigue siendo plano.
- El `.r2i` de un módulo puede referenciar tipos de terceros en su API pública; su
  cabecera `// imports:` lista esos módulos y el importador los carga recursivamente. Un
  ciclo de imports, dos módulos con el mismo id codificado y un módulo que se importa a
  sí mismo son error.
- Además, la cabecera del `.r2i` y de la `.h` incluye `// depends:` (las librerías de la
  sección anterior) y `// total_imports:` con **todos** los módulos importados, directos
  y transitivos, con el nombre del módulo tal cual se escribe en el fuente (`/` entre
  segmentos). A diferencia de `imports:`, no se limita a los módulos que aparecen en
  la API pública,
  así que el gestor de proyectos puede recorrerlos y reunir los `depends` de cada uno
  sin que importe qué símbolos sean públicos. Las dos líneas se emiten siempre, aunque
  estén vacías.
- Inicialización: si el `.r2i` de un import directo dice `has_dynamic_init: 1`, el
  `__r2_init_<mod>` del importador llama primero a ese init. Cada init lleva una
  guarda (global privada `__r2_init_guard_<mod>`) y se ejecuta **como máximo una
  vez**, también en un grafo diamante o si un host C lo llama dos veces. Un init
  vacío no emite símbolo, no se declara en el `.r2i` y no se llama.
- El formato del `.r2i` es una cabecera de comentarios (`// r2i v1`, `source`, `module`,
  `has_dynamic_init`, `imports:`, `depends:`, `total_imports:`) seguida de declaraciones R2
  con los nombres ya manglados.
  Es un artefacto del compilador: no se edita a mano.

---

## 10. Condiciones

- Toda condición (`if`, `elif`, `while`, `for`) es una expresión de tipo no-`void` (un cast vale como condición: `if (x as i64)` es válido si `x` no es `void`).
- Cualquier valor **distinto de cero es verdadero**; cero es falso.
- Un **puntero** (y `NULL`) es una condición válida: `if (p)` es falso si `p` es `NULL`. `NULL` es una constante, así que advierte (`condition is always false`). Un **literal string** no es una condición válida (`expression with resulting type string is not valid as a condition`).
- La cond vacía de `for` cuenta como verdadera.
- Una condición constante (literal o expresión plegada a literal, también tras un cast: `if (1)`, `if (0)`, `if (1 == 1)`, `while (0)`) emite un **warning** (`condition is always true` / `condition is always false`). La cond vacía de `for` no advierte.

---

## 11. Entry point y salida

- El entry point es exactamente `pub fn main() : i64`, sin parámetros. Su valor de retorno es el exit code del programa.
- Los inits dinámicos de las globales se ejecutan antes de `main` (ver la sección 9).
- Los prefijos `__r2_` y `_r2_` están reservados a nivel top-level y los rechaza el compilador (`identifiers starting with '__r2_' are reserved` / `identifiers starting with '_r2_' are reserved`). Las locales y los parámetros pueden empezar por ellos.
- Si el programa no define `main`, se genera assembly igualmente.
- La salida del compilador es **assembly x86-64** AT&T, no un binario. El binario final se produce ensamblando (p. ej. `gcc prog.s -o prog`).

---

## 12. Comportamiento indefinido

No se añade ninguna lógica para evitarlo; el resultado es lo que haga el hardware:

- Shifts con cuenta `>= 64` (una cuenta negativa se interpreta como un valor sin signo enorme, así que cae en este caso).
- División o módulo por cero en runtime (divisor no conocido en compilación): fault del hardware (`SIGFPE`). Un divisor cero conocido en compilación (literal, plegado o tras un cast) es error de compilación.
- División o módulo con el mínimo negativo de un narrow (`INT8_MIN / -1`, `INT16_MIN / -1` y análogos): fault del hardware (`#DE`), porque la división corre al ancho nativo del tipo. También cuando es constante: sema no la pliega (a diferencia de otras constantes, que sí se pliegan), así que el fault es el mismo que en runtime.
- Leer una local sin asignar: valor indefinido. El compilador no garantiza ninguna invariancia para esas lecturas: el valor puede diferir entre niveles de optimización (p. ej. una constante condicional que antes se materializaba en todo uso ya no lo hace).
- **Desreferenciar un puntero inválido** (`NULL`, uno sin inicializar, o uno que apunte a memoria que el programa no posee): no se comprueba nada, es responsabilidad del programador. La aritmética de punteros y la indexación `p[i]` tampoco se validan contra ningún límite.
- **Memoria dinámica mal usada**: `delete` de un puntero que no venga de un allocator compatible con `free` (p. ej. un slice que apunta a un array del stack o a datos de un `new(mi_alloc(...))` liberado con `delete x;`), un `free` doble, usar memoria después de `delete`, o desajustar el allocator de `new(expr) ...` con el deallocator de `delete(expr) x;`. Tampoco hay seguimiento del ciclo de vida: destruir dos veces (`drop` y luego `delete`, dos `drop`, un `reset` de un objeto ya destruido), leer después de `drop` o usar un objeto que nunca se inicializó es comportamiento indefinido si el destructor depende de esos bytes. No hay comprobaciones de `NULL` ni de propiedad.
- **Llamar a un `ptr fn` inválido** (`NULL`, uno sin inicializar, o uno cuya firma real no coincide con la del tipo): no se comprueba nada en runtime. La llamada salta a la dirección guardada y aplica la ABI según la firma **declarada**, así que un cast explícito entre firmas distintas (§8.2) compila y puede corromper los argumentos o el retorno; verificar la firma es responsabilidad del programador. Las llamadas indirectas tampoco se convierten en tail calls.

El **wrap** aritmético NO es indefinido: está definido como complemento a 2 (y módulo 2^64 para `u64`). Tampoco lo son los shifts con cuenta menor que 64: se calculan a 64 bits sobre el valor extendido y el resultado se trunca al tipo del operando izquierdo (p. ej. `u8 << 9` vale 0 y `i8 >> 9` rellena con el signo).

Plegado de constantes: una subexpresión hecha solo de literales se evalúa en compilación con esa misma semántica de wrap, siempre a 64 bits (ver "Ancho del plegado"), incluida la negación: `-5u` es la constante `u64` exacta, sin warning. Los literales detrás de un cast explícito también pliegan, truncando primero al tipo del cast (`-(300 as u8)` es la constante `212`). El divisor cero conocido (literal, plegado o tras un cast) es error de compilación (`division by zero`; `division by zero in constant expression` si ambos operandos son literales). No se pliegan y se dejan al hardware: `INT64_MIN / -1` (o `%`), el mínimo de un narrow dividido por `-1` y los shifts con cuenta `>= 64`.

---

## Warnings

Sema emite los siguientes warnings (no bloquean la compilación; no afectan a los exit codes ni a los dumps):

- Literal que no cabe en 64 bits (truncado).
- Variable, parámetro, función, tipo (`struct`/`union`/`enum`/`type`/`newtype`), campo de struct o variante de enum no usada. Contar como uso cualquier mención del nombre (init, lectura, escritura, `offsetof`, `sizeof`/`alignof` del campo). Las declaraciones `pub`/`export` nunca avisan (ni los campos/variantes de un tipo `pub`), ni los nombres que empiezan por `_`. Un tipo que no se usa avisa una sola vez, no por cada campo; los campos de un struct alcanzable desde C a través de un `extern`/`export` tampoco avisan.
- Cuenta de shift literal fuera del ancho del tipo (o negativa): `shift count N is out of range for type T (W bits)` / `negative shift count`.
- Comparación de un valor sin signo con 0 que es siempre verdadera o falsa (`x < 0`, `x >= 0`, `0 > x`, `0 <= x`).
- Variable, campo o elemento usado sin asignar.
- Valor asignado no usado (variable, campo o elemento).
- Cast redundante (el valor ya tiene ese tipo).
- Autoasignación (`x = x`).
- Expresión sin efecto (`expression has no effect; its result is discarded`).
- Condición siempre verdadera/falsa.
- Operando constante en `&&`/`||`.
- Comparación de una expresión consigo misma.
- Negación de un valor `unsigned`.
- Desreferencia de un puntero conocido `NULL`.
- Resta de dos literales string.
- Literal `u` usado como tipo con signo.
- Valor truncado en un bitfield.
- Código inalcanzable.
- Retorno de un puntero o slice a un local (storage en el stack).

## 13. Optimizaciones

`-O` selecciona el nivel (defecto `2`):

- `-O0`: sin optimizar; todos los valores viven en pila (spill-all).
- `-O1`: IR sin optimizar; el asignador de registros colorea por bloques y
  reparte los valores entre registros físicos y pila (sin tocar la IR).
- `-O2`: optimiza la IR antes del codegen (pases BASIC) y después asigna registros.
- `-O3`: como `-O2` más las optimizaciones caras (CSE, DCE global, inlining y
  `select`).

Optimizaciones de la IR (activas en `-O2` y `-O3`):

- **Propagación**: sustituye copias y constantes por su valor.
- **Plegado de constantes**: evalúa en compilación aritmética, bitwise, comparaciones y saltos con condición constante.
- **Simplificación algebraica**: `x - x` → `0`, `x * 0` → `0`, `x * 1` → `x`, `x % 1` → `0`, `x + 0` → `x`, `x ^ x` → `0`, etc.
- **Strength reduction**: convierte `*`, `/` y `%` por potencias de 2 en shifts y máscaras.
- **Eliminación de código muerto**: instrucciones cuyo resultado no se usa.
- **Bloques inalcanzables y saltos**: quita bloques inalcanzables, encadena saltos, invierte ramas y elimina etiquetas sin referencias.
- **Globales**: forwarding de lecturas dentro del mismo bloque y eliminación de stores muertos. Un store a través de un puntero solo invalida las globales **visibles por puntero** (las que tienen algún `$g` en el programa y las `extern`); el resto mantiene su valor forwardeado. Se asume que no se llega a una global desde otra por aritmética de punteros, como en C.
- **Coalescing**: fusiona una operación con la copia que la sigue.
- **SROA de structs pequeños** (opt_sroa), **reenvío de campos** (opt_fields), **plegado de desplazamientos** (opt_disp) y **de direcciones de frame** (opt_frame).
- **Inits estáticos**: una global inicializada con una constante pasa a `.data` (sin código de arranque), incluidos los arrays y structs cuyo inicializador es 100 % constante (imagen de bytes). Un `ptr fn` inicializado con el **nombre de una función** también se liga (`.quad <símbolo>`). Los inicializadores con punteros, slices, bitfields o cualquier expresión no constante siguen escribiéndose desde `__r2_init`; una función que contiene una llamada indirecta se considera que toca **todas** las globales, así que un `call` a ella (dentro de `__r2_init`) impide ese plegado en las globales que siguen.

Solo en `-O3`:

- **CSE**: reutiliza `*`, `/` y `%` repetidos con el mismo divisor no constante.
- **DCE global**: eliminación de código muerto con liveness global, no solo local.
- **Inlining**: inlinea funciones hoja de hasta 24 instrucciones.
- **`select`**: if-conversion de `if (c) x = a; else x = b;` con ramas simples y, también, del ternario `c ? a : b` cuando la condición y ambas ramas son puras (no aparece en `--dump-ir`; sí en `--dump-opt`). Un ternario con ramas impuras baja a saltos y evalúa solo la rama elegida.

Las declaraciones `extern` quedan fuera de las optimizaciones: una función `extern` no tiene cuerpo (no se optimiza ni se inlinia) y una `extern var` nunca se considera constante, porque cualquier llamada a C puede modificarla.

El codegen aplica además optimizaciones locales (operaciones read-modify-write, fusión de comparación con salto, tail calls, shrink-wrapping de registros callee-saved) en todos los niveles; `-O0` fuerza que los valores vivan en pila.

---

## 14. Gramática (EBNF)

```
program        = { top_decl } ;
top_decl       = [ "pub" | "export" ] ( fn_decl | var_decl | arr_decl | enum_decl
                           | type_decl | newtype_decl | struct_decl | class_decl )
               | extern_decl
               | module_decl
               | depends_decl
               | import_decl
               | from_decl ;
extern_decl    = "extern" ( "fn" IDENT "(" [ params ] ")" [ ":" ret_type ] ";"
                          | "var" IDENT ":" type ";" ) ;
module_decl    = "module" STRING_LIT ";" ;
depends_decl   = "depends" STRING_LIT { "," STRING_LIT } ";" ;
module_name    = STRING_LIT ;
import_decl    = "import" module_name [ "as" IDENT ] ";" ;
from_decl      = "from" module_name "import" from_item { "," from_item } ";" ;
from_item      = IDENT [ "as" IDENT ] ;
fn_decl        = "fn" IDENT "(" [ params ] ")" [ ":" ret_type { "," ret_type } ] block ;
params         = param { "," param } ;
param          = "self"
               | IDENT ":" [ "&" | "..." ] type [ "=" expr ] ;
ret_type       = "void" | type ;
type           = type_atom { "[" [ expr ] "]" } ;
type_atom      = [ "const" ] ( "(" type ")" | "ptr" type | fn_type | builtin_or_ident ) ;
fn_type        = "fn" "(" [ fn_param_list ] ")" [ ":" fn_ret ] ;
fn_param_list  = "..." | fn_param { "," fn_param } [ "," "..." ] ;
fn_param       = [ IDENT ":" ] ( "&" type | type ) ;
fn_ret         = "(" type { "," type } ")" | type ;
enum_decl      = "enum" IDENT [ ":" type ] "{" enum_val { "," enum_val } [ "," ] "}" [ ";" ] ;
enum_val       = IDENT [ "=" expr ] ;
type_decl      = "type" IDENT "=" type ";" ;
newtype_decl   = "newtype" IDENT "=" type ";" ;
struct_decl    = [ "packed" ] ( "struct" | "union" ) IDENT "{" { field_decl | struct_decl } "}" [ ";" ] ;
class_decl     = "class" IDENT "{" { field_decl | struct_decl | method_decl
                                   | init_decl | destroy_decl } "}" [ ";" ] ;
method_decl    = [ "priv" ] fn_decl ;
init_decl      = "init" "(" "self" [ "," param { "," param } ] ")" block ;
destroy_decl   = "destroy" "(" "self" ")" block ;
field_decl     = IDENT ":" ( bitfield_type | type [ ":" INT_LIT ] ) ";" ;
bitfield_type  = ( "u" | "i" ) INT_LIT ;
block          = "{" { statement } "}" ;
statement      = var_decl | arr_decl | static_decl | block | if_stmt | while_stmt | for_stmt
               | switch_stmt
               | "return" [ expr { "," expr } ] ";"
               | jump_stmt
               | multi_assign
               | inc_dec_stmt
               | delete_stmt
               | drop_stmt
               | reset_stmt
               | expr ";" ;
jump_stmt      = ( "break" | "continue" ) [ INT_LIT ] ";" ;
delete_stmt    = "delete" [ "(" expr ")" ] expr ";" ;
drop_stmt      = "drop" expr ";" ;
reset_stmt     = "reset" lvalue "(" [ args ] ")" ";" ;
multi_assign   = ( lvalue { "," lvalue } | multi_decl { "," multi_decl } ) "=" expr ";" ;
multi_decl     = ( "var" | "arr" ) IDENT ":" type ;
inc_dec_stmt   = lvalue ( "++" | "--" ) ";" ;
lvalue         = IDENT | "@" unary | postfix ;
var_decl       = "var" IDENT ":" type [ "=" expr ] ";" ;
arr_decl       = "arr" IDENT ":" type [ "=" expr ] ";" ;
static_decl    = "static" ( var_decl | arr_decl ) ;
if_stmt        = "if" "(" expr ")" statement
                 { "elif" "(" expr ")" statement }
                 [ "else" statement ] ;
while_stmt     = "while" "(" expr ")" statement [ "else" statement ] ;
switch_stmt    = "switch" "(" expr ")" "{" { case_clause | default_clause } "}" ;
case_clause    = "case" "(" expr { "," expr } ")" statement ;
default_clause = "default" statement ;
for_stmt       = "for" "(" for_init for_cond for_updt ")" statement [ "else" statement ]
               | "for" "(" for_range ")" statement [ "else" statement ] ;
for_init       = var_decl | [ expr ] ";" ;
for_cond       = [ expr ] ";" ;
for_updt       = [ upd { "," upd } ] ;
upd             = expr | lvalue ( "++" | "--" ) ;
for_range      = "var" IDENT ":" type { "," IDENT ":" type } [ "," IDENT ] ":" expr
               | "var" IDENT { "," IDENT } ":" expr
               | IDENT { "," IDENT } ":" expr ;
expr           = assignment ;
assignment     = ternary [ ( "=" | "+=" | "-=" | "*=" | "/=" | "%="
                            | "&=" | "|=" | "^=" | "<<=" | ">>=" ) assignment ] ;
ternary        = logic_or [ "?" expr ":" ternary ] ;
logic_or       = logic_and { "||" logic_and } ;
logic_and      = bit_or { "&&" bit_or } ;
bit_or         = bit_xor { "|" bit_xor } ;
bit_xor        = bit_and { "^" bit_and } ;
bit_and        = equality { "&" equality } ;
equality       = relational { ( "==" | "!=" ) relational } ;
relational     = shift { ( "<" | ">" | "<=" | ">=" ) shift } ;
shift          = additive { ( "<<" | ">>" ) additive } ;
additive       = multiplicative { ( "+" | "-" ) multiplicative } ;
multiplicative = cast { ( "*" | "/" | "%" ) cast } ;
cast           = unary { "as" type } ;
unary          = ( "-" | "!" | "~" | "@" | "$" ) unary
               | "sizeof" "(" ( type | sizeof_arg ) ")"
               | "alignof" "(" ( type | sizeof_arg ) ")"
               | "offsetof" "(" type "," IDENT { "." IDENT } ")"
               | postfix ;
postfix        = primary { "[" expr "]" | "." IDENT | "(" [ args ] ")" } ;
sizeof_arg     = "@" unary | postfix ;
primary        = INT_LIT | CHAR_LIT | STRING_LIT | "NULL"
               | IDENT | IDENT "(" [ args ] ")"
               | "new" [ "(" expr ")" ] type_atom ( { "[" expr "]" } | "(" [ args ] ")" )
               | "(" expr ")"
               | "{" [ init_item { "," init_item } ] "}" ;
init_item      = expr
               | "." IDENT "=" expr
               | "[" expr "]" { "[" expr "]" } "=" expr ;
arg            = expr [ "..." ] ;
args           = arg { "," arg } ;
```

Notas: `var_decl` dentro de `for_init` incluye su propio `;`. Los literales se definen en la sección 2. `char` es alias de `i8` en cualquier posición de `type`. `pub` y `export` solo preceden a declaraciones top-level: `pub` vale para todas menos `static`, `export` vale para las mismas salvo `extern` e implica `pub` (§9.1). Un `type` puede ser una **firma de función** (`ptr fn(...)`, §8.2): `ptr fn(a : i32) : i32` es un puntero, `fn(...)` sin `ptr` es idéntico y los varios retornos van entre paréntesis (`fn(a : i32) : (i32, u32)`); el `...` final es el variádico estilo C, cada parámetro puede omitir su nombre y `void` no vale como tipo de parámetro ni de retorno (`expected type`). El sufijo `(` de `postfix` es una **llamada indirecta** sobre un valor `ptr fn` (§8.2) y se encadena como los demás. Por leniencia el parser acepta una coma final en parámetros (`fn f(a : i64,)`) y argumentos (`f(a,)`), aunque la EBNF no la muestra. En una lista de argumentos, una asignación `IDENT = expr` en el nivel superior se interpreta como **argumento con nombre** (`f(3, b = 10)`), no como una asignación posicional; el nombre debe coincidir con un parámetro. El cast es postfijo y asociativo por la izquierda (`x as i64 as u64` = `(x as i64) as u64`); el tipo tras `as` nunca puede ser `void`. En `type`, un `IDENT` se resuelve contra los enums, los structs/uniones y las declaraciones `type`/`newtype` (también las declaradas más tarde); un nombre no declarado es `unknown type 'Foo'`. `enum_decl` exige al menos una variante (el parser acepta `{}` para que sema emita `enum 'E' must have at least one variant`); la coma final y el `;` tras `}` son opcionales. El valor de una variante debe ser un literal, `-literal`, `~literal` o un cast de esas formas; sema rechaza cualquier otra expresión. En `IDENT "." IDENT` y, en general, en `expr "." IDENT`, el `.` puede ser acceso a campo de un struct/unión (ver §3.4) o acceso a variante de un enum (ver §3.1): si el lado izquierdo es un nombre cuyo tipo representado sea un enum (alias o newtype), el `.` resuelve a una variante `Tipo.Variante`, una constante de compilación; en cualquier otro caso es acceso a campo. La indexación es un sufijo postfijo que se encadena (`p[i][j]` = `(p[i])[j]`) y se aplica antes que el unario (`-p[0]` = `-(p[0])`). `@` y `$` son operadores prefijos del nivel `unary`, así que `@p + 1` es `(@p) + 1`; `sizeof` también es prefijo y su operando está restringido a `type`, `IDENT`, `@` sobre una expresión unaria, una indexación o un literal string (cualquier otra cosa es `operand of sizeof must be a variable, dereference, index, string literal or type`). La recursión de `ptr` en `type` también está limitada por la profundidad del parser (`type nested too deeply`). El objetivo de `++`/`--` puede ser una variable, una desreferencia (`@p++`) o una indexación (`p[i]++`), nunca una dirección (`$x++`). En `for_range` lo que sigue al `:` se lee primero como tipo y, si lo que viene después es `)`, se reinterpreta como la expresión iterada: así `var i : lista` no se confunde con `var i : T`, y `T[i]` en esa posición se reinterpreta como indexación. Tras `var IDENT`, un `:` abre la forma con tipos y una `,` la forma sin ninguno (los índices sin tipo son `i64`); el último índice de la forma con tipos puede omitir el suyo. Los bucles de array quedan desazucarados en el propio AST (uno por índice, anidados), de modo que el `for` resultante es idéntico al escrito a mano. El ternario se analiza entre la asignación y `||` (ver §4): su rama `then` es una expresión completa (admite asignación) y la rama `else` se queda en el nivel del ternario, así que `a ? b : c = d` es `(a ? b : c) = d`. `switch` exige un `default`; cada valor de un `case` es una expresión constante, el cuerpo de cada cláusula es un statement cualquiera (normalmente un bloque) y no hay fallthrough; un segundo `default` es error (`duplicate default clause`). `static` solo aparece dentro de una función y su inicializador debe ser una expresión constante (ver §6). `arr_decl` usa `arr` para almacenamiento; un `var IDENT : T[N]...` local declara una vista no propietaria (no equivale a `arr`; ver §3.3), y `arr` exige que el tipo sea array (`'arr' declarations must have an array type`). `sizeof`, `alignof` y `offsetof` aceptan un tipo o una expresión con las restricciones de §5 y se pliegan en sema. En un `{...}`, cada elemento es posicional, `.campo = valor` o `[índice] = valor`; las tres formas no se mezclan (`cannot mix '.field = value', '[index] = value' and positional initializers`). Un `{...}` de expresión es un literal de struct/unión (nunca de clase, §3.7) y solo vale con un `as type` que lo acompañe (`cast` ya lo cubre); sin `as` sema da `a '{...}' literal needs an explicit type; write '{...} as T'`. Los bitfields se escriben con el ancho (`u8 : 3`) o con un entero sin keyword (`u3`, `i5`, ...); `uN`/`iN` son identificadores, no keywords. Un `else` tras un `while`/`for` pertenece al bucle o `if` más interno que lo pueda consumir: si el cuerpo es un `if`, lo consume ese `if`; si es otro bucle, ese bucle. `any` es un tipo builtin (§3.4) y, como `module`, no puede usarse como identificador. En `param`, el prefijo `...` marca el variádico tipado (`rest : ...i64`, que dentro es `rest : i64[]`), solo en `fn` normales y siempre como último parámetro; el `...` **sin tipo** de `extern` (`extern fn f (a : T, ...)` o `extern fn f (...)`) no se muestra en esta gramática y se describe en §8.1. El `...` de `arg` es el *spread* de un slice en una llamada variádica tipada (§8) y debe ser el último argumento. `module_decl` es único y solo top-level (§9.1). `import_decl` y `from_decl` pueden aparecer en cualquier punto top-level y valen para todo el fichero (§9.2). Un `qual_path` (`IDENT { "::" IDENT }`) es un nombre cualificado de módulo: se acepta donde se acepta un `IDENT` en un `type` (incluidos `ptr`, arrays, firmas `fn` y casts), en expresiones (`m::f(...)`, `m::g`, `m::E.Var`, `$m::g`, `sizeof(m::T)`) y en `import`/`from` (`std::math::abs`, `from std::math import abs`). `class_decl` (§3.7) acepta campos como un struct, structs anidados, métodos `fn`, y las formas contextuales `init (...) { }` y `destroy (...) { }` (solo dentro de una clase y solo si el identificador va seguido de `(`; `init`/`destroy` no son keywords). Solo el primer parámetro de un método, del `init` o del `destroy` puede ser el identificador `self` sin tipo, y `priv` solo precede a un método dentro de una clase. `pub`/`export class` se parsean pero sema los rechaza por ahora. Una llamada `IDENT(args)` cuyo nombre resuelve a una clase es una **construcción** (§3.7); los literales `{...}` de una clase son error. `new` (§3.6) reserva memoria: su tipo base es un `type_atom` (sin sufijos) y cada `[...]` posterior es una dimensión de la reserva; los paréntesis inmediatamente tras `new` son el **allocator** (`new(expr) T[N]...`) y los que siguen al tipo, si es una clase, son argumentos de `init` (`new Clase(args)`), así que un tipo base que necesite paréntesis en la posición del allocator se escribe mediante un alias. `delete` (§3.6) acepta un operando (`delete x;`) o un deallocator (`delete(expr) x;`), y `delete(x);` libera `x` porque no hay nada tras el paréntesis. `drop` y `reset` (§3.7) son keywords de sentencia: `drop expr;` y `reset lvalue(args);`; el target de `reset` se analiza como un lvalue (variable, `@`, campo o índice) y los argumentos usan la gramática normal de llamada.

---

## 15. Uso del compilador

```bash
make
./build/r2p [OPTIONS] codefile.r2
```

| Flag | Efecto |
|------|--------|
| `-T` / `--dump-tokens` | Vuelca los tokens a stdout |
| `-A` / `--dump-ast` | Vuelca el AST a stdout |
| `-S` / `--dump-symbols` | Vuelca la tabla de símbolos resuelta a stdout |
| `-I` / `--dump-ir` | Vuelca la IR generada a stdout |
| `-D` / `--dump-opt` | Vuelca la IR optimizada a stdout (sin `-O`, optimiza al nivel máximo; con `-O0`/`-O1` la muestra sin optimizar) |
| `-O[LEVEL]` / `--opt[=LEVEL]` | Nivel de optimización de `0` a `3` (sin espacio; `-O` solo = `2`). Defecto: `2`; ver la sección 13 |
| `-o` / `--out` | Ruta del assembly de salida (por defecto `<fuente>.s`; `-` = stdout) |
| `-m` / `--modules-dir DIR` | Directorio de los `.r2i` generados (defecto `modules`) |
| `-p` / `--path DIR` | Directorio extra donde buscar `.r2i` importados (repetible) |
| `-n` / `--no-r2i` | No genera el `.r2i` de este módulo |
| `-H[DIR]` / `--header[=DIR]` | Genera la cabecera C/C++ de lo `export` en `DIR` (defecto `include`); sin `export` no escribe nada |
| `-a` / `--arch ARCH` | Arquitectura: `x86-64` (alias `x86_64`, `amd64`), `arm`/`aarch64`, `riscv`/`riscv64`. Solo x86-64 está implementado |
| `-e` / `--execute` | Modo intérprete (no implementado) |
| `-v` / `--version` | Muestra la versión |
| `-h` / `--help` | Muestra la ayuda |

Nota: sin flags de dump, `r2p` compila y escribe el assembly a `<fuente>.s` (o a la ruta de `-o`; `-` es stdout). `-a arm`, `-a riscv` y `-e` abortan en codegen con `Error: <backend> backend is not implemented` (`ARM`, `RISC-V` o `interpreter`). Los modos de dump no pasan por codegen, así que pueden inspeccionar un programa aunque la arquitectura elegida no tenga backend. Los punteros y los literales string están completos de punta a punta: la aritmética (`p ± n`, `p - q` en elementos, `p++`/`p--`, `p += n`), los casts entre punteros, `@p`, `$x`, `NULL` y los literales string se analizan y se generan a assembly (los strings van a una piscina deduplicada en `.rodata`; las locales cuya dirección se toma reciben slot de pila; los inits globales de puntero que son constantes de enlace — `NULL`, string o `$g` — se ligan estáticamente en `.data`). Si se pasan varios ficheros, solo se compila el primero y se avisa por stderr.

El nombre de módulo que se usa para manglar los símbolos top-level (§9.1) depende de la ruta: se toma la ruta del fuente relativa al ancestro común entre el directorio actual y el del fichero (o el basename si solo comparten `/`), con el **directorio** canonicalizado (se resuelven `.`, `..` y symlinks de directorio) y el **nombre del fichero tal cual se escribe** (un symlink de fichero conserva su nombre), se quita `.r2` y se codifica cada segmento como `<longitud escapada><bytes escapados>` (`src/math.r2` → `3src4math`, `_r2_pf_3src4math_3add`). Por eso el mismo fichero compilado desde otro directorio puede producir símbolos distintos; para símbolos estables entre ejecuciones y enlazables desde C usa `export` (nombre plano).

Para ejecutar un programa:

```bash
./build/r2p prog.r2      # escribe prog.s
gcc prog.s -o prog       # ensambla y enlaza
./prog; echo $?          # el exit code es el valor que devuelve main
```

Ejemplo de programa completo:

```r2
// factorial iterativo

fn fact (n : i64) : i64
{
	var r : i64 = 1;
	var i : i64 = 2;

	while (i <= n) {
		r = r * i;
		i = i + 1;
	}

	return r;
}

var start : i64 = 5;

pub fn main () : i64
{
	var f : i64 = fact(start);
	if (f == 120) {
		return 0;
	}
	return 1;
}
```

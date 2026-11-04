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
- Punteros (`ptr T`, `ptr void`), `$x` dirección, `@p` desreferencia, `p[i]` indexación, el literal `NULL`, `sizeof`, aritmética y comparación de punteros y casts solo entre punteros. Ver §3.
- Literales string, de tipo `string`, que se comportan como `ptr i8` donde se espera uno. Ver §2.5.
- Funciones solo a nivel top-level, con parámetros por valor.
- Interoperabilidad con C vía declaraciones `extern fn` / `extern var`, que no llevan cuerpo ni init y se emiten con su nombre C sin mangling. Ver §8.1.
- Scopes de bloque con shadowing entre scopes.
- Entry point: `fn main() : i64`.
- El compilador emite assembly x86-64 AT&T (no binarios: el binario final se produce ensamblando, p. ej. `gcc prog.s -o prog`). Una compilación sin flags de dump escribe `<fuente>.s` (la ruta del fuente con la extensión cambiada por `.s`) o la ruta de `-o`.

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
fn  if    for    i8   u8   i16  u16  i32  u32
i64  u64   var   char  elif  else  void
while  return  as  enum  type  newtype  extern
ptr  NULL  sizeof
```

`ptr`, `NULL` y `sizeof` son keywords (no identificadores). `@` y `$` son operadores unarios, no palabras clave.

### 2.3 Literales enteros

- Decimal: `42`. Hexadecimal: `0x10` / `0X10`. Octal: `0o17` / `0O17`. Binario: `0b101` / `0B101`.
- Los decimales con ceros a la izquierda (`007`) son decimales, no octales. `0` solo es decimal.
- Sufijo `u` / `U` → literal `u64`; sin sufijo → literal `i64` en crudo (el valor se guarda sin interpretar el signo; es sema quien lo tipa según el contexto).
- Desbordar los 64 bits en cualquier base no es error: el lexer emite un **warning** y trunca el valor a 64 bits (`literal does not fit in 64 bits, truncated to …`). Dígito inválido para la base o prefijo sin dígitos: error.
- `2^63` sin sufijo (`9223372036854775808`, `0x8000000000000000`, `0o1000000000000000000000`, `0b1` + 63 ceros) se lexa como `TOK_LIT_i64` con el valor en crudo; el parser lo deja tal cual (incluso tras `-`: `-9223372036854775808` es `NEG(lit)` en el AST) y es **sema** quien lo resuelve: `-2^63` se pliega a un único literal `INT64_MIN` válido, un `2^63` positivo suelto es error (`literal … does not fit in type i64; it is too large (the 'u' suffix makes it a u64 literal)`), y en contexto `u64` el literal adopta `u64` en silencio.

### 2.4 Literales char

- `'c'`, exactamente un carácter. Vacío, multichar, newline dentro o sin cerrar: error.
- Escapes válidos: `\n` `\t` `\r` `\0` `\\` `\'` `\"`. Escapes desconocidos: error.
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
| `ptr T` | 8 bytes | Puntero a `T`. |
| `ptr void` | 8 bytes | Puntero sin tipo apuntado. Convierte **en silencio** desde y hacia cualquier `ptr T` (init, asignación, argumento, `return`); sin aritmética ni desreferencia. |
| `T[N]` | `N * sizeof(T)` | Array concreto de `N` elementos de tipo `T`, contiguo (row-major). Solo se declara con `arr`; `N` es una constante entera > 0. |
| `T[]` | 16 bytes (ABI) | Slice/vista de `T`: puntero a los datos + longitud. Solo puede aparecer como tipo de parámetro. |
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
- **Cast explícito `expr as type`** (`type` = cualquier numérico, incluido `char` como alias de `i8`, un alias de tipo, un enum o un newtype): acepta cualquier operando no-`void` (incluido el resultado de otro cast) y devuelve exactamente el tipo escrito. Reinterpreta/trunca bits en silencio. Con enums, el cast explícito está permitido entre un enum y cualquier entero (incluido su subyacente), pero no entre dos enums distintos (ver §3.1). Con newtypes, el cast explícito está permitido entre un newtype y cualquier entero o enum, pero no entre dos newtypes distintos (ver §3.2). `as void` no existe (error de parser: `expected type`). Castear un `void` (p. ej. `f() as i64` con `f : void`) es error de sema (`cannot cast a value of type void`). Castear un operando con error (no declarado, string, etc.) no añade un segundo error: el cast propaga `error`. Un cast que involucre un puntero solo vale entre punteros (`p as ptr i64`, `p as ptr void`): ver el bloque **Punteros** más abajo.
- `void` no puede aparecer en expresiones (ni en condiciones, ni como operando, ni como init, ni como origen/destino de cast).

**Punteros.** `ptr T` con `T` = cualquier tipo entero, `enum`, `newtype`, alias de esos, `void` o **otro puntero** (`ptr ptr i64`): la base se resuelve recursivamente (los alias se canonicalizan primero, así que `type P = ptr i64; var q : ptr P;` es `ptr ptr i64`) y `ptr` es idempotente sobre el mismo `T`. Un `ptr` de un tipo no resoluble o no válido es error (`invalid pointer base type 'X'`).

- **`$x` (dirección de)**: sobre una variable **escalar** (`NODE_ID`) — local, global o parámetro —, sobre una desreferencia (`$@p`, que devuelve el propio `p`) o sobre una indexación (`$p[i]`, que devuelve `p + i`). Da `ptr T` con el tipo del operando. Sobre un array o un slice en crudo es error: el array suelto ya decae a `ptr T` a su primer elemento, así que usa `a` directamente en lugar de `$a`. `$f` (función) es error (`'f' is a function, not a variable`) y cualquier otro operando también (`operand of '$' must be a variable, a dereference or an index`).
- **`@p` (desreferencia)**: `p` debe ser `ptr T`; da un *lvalue* de tipo `T`, así que vale como valor y como destino de asignación/`++`/`--`/compuesto (`@p = 7;`, `@p++;`, `@p += 1;`). `@pv` con `p : ptr void` es error (`cannot dereference a value of type ptr void`); `@x` con `x` no puntero, error. Desreferenciar un puntero sin asignar emite el warning normal de "usado sin asignar". Si en ese punto el puntero es conocido `NULL` (su init o su última asignación incondicional fue `NULL` y ningún camino vivo lo reasigna), `@p` emite un **warning** (`null pointer dereference: 'p' is NULL here`); el análisis es conservador y sensible al flujo, igual que el de "asignada" (§7).
- **`p[i]` (indexación)**: `p` debe ser `ptr T` (no `ptr void`) e `i` un valor entero, que se finaliza a su tipo por defecto. Es exactamente `@(p + i)`: un *lvalue* de tipo `T` con el offset escalado por `sizeof(T)` (en la IR, `p[i]` baja a `mul` por el tamaño del elemento + `add` + `ld_ptr`/`str_ptr`), así que vale como valor y como destino (`p[i] = 1`, `p[i]++`, `p[i] += 2`, `$p[i]`). Se encadena por la izquierda: `pp[i][j]` es `(pp[i])[j]`. `p[i]` con `p : ptr void` es error (`cannot index 'ptr void'; cast it to a typed pointer first`), indexar un no-puntero también (`cannot index a value of type T`) y un índice no entero también (`pointer indexing requires an integer index`). Un puntero conocido `NULL` emite el mismo warning que al desreferenciar. Un literal string **no** es indexable directamente (igual que no es desreferenciable): `"abc"[i]` es error. No se comprueban límites.
- **`NULL`**: es el único valor de tipo `nil`. Se convierte **en silencio a cualquier `ptr T`** (init, asignación, argumento, `return`) y se compara con cualquier puntero. No es un entero: `p == 0`, `p + 1` con `0`, `NULL as i64` y `5 as ptr i64` son error.
- **Aritmética**: `p + n` y `p - n` (con `n` de cualquier tipo entero, que se finaliza a su tipo por defecto) dan `ptr T`; `n + p` también. `p - q` con **el mismo** tipo puntero da `i64`: la distancia en **elementos**, como en C (`(p + 1) - p` vale 1; con `ptr i8` un elemento es un byte). Un array o un slice en cualquiera de los dos lados decae antes a su puntero de datos, así que `a + 1`, `1 + a`, `a - 1` y `a - b` valen (§3.3). Tipos distintos son error (`pointer type mismatch`) y `ptr void` no admite ninguna aritmética (`pointer arithmetic on 'ptr void' is not allowed`). El sema solo tipa; la IR aplica el **escalado** (`p + 1` avanza `sizeof(T)` bytes; también en `p - n`, `n + p`, `p++`/`p--` y `p += n`/`p -= n`) y la **desescalada** de `p - q` (dividiendo por `sizeof(T)`, con signo) mediante `ptr_scale` en `src/ir/ir.c`. Con `sizeof(T) == 1` el escalado se omite.
- **Comparaciones**: `==`/`!=`/`<`/`>`/`<=`/`>=` entre **el mismo** tipo puntero, o contra `NULL`. Resultado `i64`. Un literal string cuenta como `ptr i8`, así que `p == "abc"` vale con `p : ptr i8`.
- **Casts**: `expr as type` solo está permitido **entre punteros** (`p as ptr i64`, `p as ptr void`); `5 as ptr i64` y `p as i64` son error (`cannot cast between pointer and non-pointer types; only pointer-to-pointer casts are supported`). Un literal flexible **nunca** adopta un tipo puntero: `var p : ptr i32 = 7;` es error y hay que escribir `7 as ptr i32`.
- **`ptr void` ↔ `ptr T` es implícito, en las dos direcciones**: `ptr void` convierte **en silencio** a cualquier otro tipo puntero y cualquier `ptr T` convierte a `ptr void`, en init, asignación, argumento y `return`, sin `as`. Es el idioma para las funciones C de `void *`: `extern fn malloc (size : u64) : ptr void; var p : ptr i32 = malloc(40);` y `extern fn free (p : ptr void) : void; ... free(p);` compilan ambos, igual que `memcpy(dst, src, n)` con `dst : ptr void` y `src : ptr i8`. Un **array o un slice** también decaen a `ptr void` (además de a `ptr T` con el elemento como base, §3.3), así que `memcpy(dst, src, n)` acepta directamente `dst`/`src` de tipo `T[N]` o `T[]`. Dos punteros **no** relacionados (`ptr i32` → `ptr i64`) siguen siendo error (`use an explicit cast with 'as'`), y las **comparaciones** siguen exigiendo el mismo tipo puntero aunque uno sea `ptr void` (`p == q` con `p : ptr i64` y `q : ptr void` es error), porque un binario con punteros se resuelve en `check_pointer_binary`, antes de la conversión implícita.
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
- El nombre del enum es un nombre top-level normal: compite con variables/funciones en el chequeo de duplicados, no puede empezar por `__r2_` y no puede ser `main` (`'main' must be a function`).

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
- `Dias.Foo` con una variante inexistente — error (`enum 'Dias' has no variant 'Foo'`). El lado izquierdo del `.` debe ser un nombre cuyo tipo representado sea un enum: `left side of '.' must be an enum type` si no es un identificador, `'X' is not an enum type` en caso contrario. Vale un enum, un alias de enum y un newtype (o alias de newtype) sobre un enum; en los dos últimos el valor resultante tiene el tipo nominal (`newtype N = Dias; N.Lunes` es de tipo `N`, ver §3.2).

### 3.2 Aliases y newtypes

```r2
type Index = u32;          // alias transparente
newtype Seconds = u32;     // tipo nominal nuevo

type Day = Dias;           // D.Lunes es Dias.Lunes
newtype Stamp = Day;       // nominalmente distinto de Dias
```

- Solo se declaran a nivel top-level; dentro de una función o bloque es error (`types can only be declared at the top level`).
- El tipo de la derecha debe ser un tipo conocido: un primitivo (`i8`…`u64`, `char`), un enum, otro alias o otro newtype. Un nombre desconocido es `unknown type 'X'` y una referencia circular es `circular type definition 'X'`.
- Las declaraciones son **orden-independientes**: se puede usar un tipo antes de su declaración en el fichero, igual que los enums (`var x : Index = 1; type Index = u32;` compila).
- El nombre es un nombre top-level normal: compite con variables/funciones/enums en el chequeo de duplicados, no puede empezar por `__r2_` y no puede ser `main` (`'main' must be a function`).

**`type X = T` (alias transparente).** `X` no es un tipo nuevo: cualquier uso como tipo se canonicaliza a `T` y desaparece en sema, IR y codegen. Mismo criterio que `char` → `i8`.

- `type Index = u32; var x : Index = 1;` — `x` es de tipo `u32`; en `--dump-symbols` aparece `var x ... type=u32` y en los mensajes se ve `u32`, nunca `Index`.
- La declaración sí aparece en `--dump-symbols`: `type Index [l:c] depth=0 type=Index base=u32`.
- Cadenas: `type A = B; type B = u8;` → tanto `A` como `B` son `u8` (`base=u8` en el dump).
- `type D = Dias;` — `D` es exactamente `Dias`, así que `D.Lunes` funciona.
- `type A = N;` con `newtype N = u32;` — `A` es exactamente `N` (nominal, no `u32`).

**`newtype X = T` (tipo nominal).** `X` es un tipo distinto, con el mismo tamaño y signo que su representación física. Se comporta como un entero de ese ancho para el codegen, pero mezclarlo con otros tipos sin `as` es error, igual que un enum.

- La base puede ser un entero concreto (`i8`…`u64`, `char`), un enum, un alias que acabe en uno de ellos, u otro newtype. `void` y los tipos untyped no valen (`newtype underlying type must be an integer type`).
- `newtype N = Seconds;` — `N` y `Seconds` siguen siendo distintos aunque compartan representación.
- `newtype N = E;` con `enum E : u8` — tamaño 1 y mismo signo que `E`. `N.A` es válido y produce un valor de tipo `N` con los bits de `E.A` (también a través de cadenas: `newtype P = N;` → `P.A`, y `type A = N;` → `A.A`). El valor es `N`, no `E`: `var e : E = N.A;` es error y exige `as`, igual que `E.A as N` para el sentido contrario.
- `var s : Seconds = 7;` — sí: el literal flexible adopta `Seconds` si cabe en su representación (`literal 300 does not fit in type Seconds` en caso contrario).
- `s + s`, `s + 1`, comparaciones, bitwise, `++`/`--` y asignación compuesta entre valores del mismo newtype — sí, con resultado `Seconds`.
- `u32 + s`, `u32 = s`, `s = u32_var`, argumento `Seconds` en parámetro `u32`, retorno `u32` desde función que devuelve `Seconds` — no; hace falta `as`. El mensaje es el genérico (`type mismatch: Seconds vs u32 (use 'as' to convert explicitly)`, `cannot assign ...`, etc.).
- Ensanchar un newtype a un entero mayor **no** es implícito (es nominal): `var u : u64 = s;` es error aunque `Seconds` sea `u32`.
- `s as u32`, `u as Seconds`, `e as Seconds` (enum → newtype) y `s as E` — sí: el cast explícito reinterpreta/trunca como entre enteros.
- `a as B` entre dos newtypes distintos — no (`cannot cast between newtype types 'A' and 'B'; cast through the underlying integer type`).
- `Seconds` usado como valor o como función — error (`'Seconds' is a type, not a variable` / `'Seconds' is a type, not a function`).
- En `--dump-symbols`: `newtype Seconds [l:c] depth=0 type=Seconds base=u32`.

### 3.3 Arrays y slices

```r2
arr lista : i32[5] = {2, 4, 6, 8, 10};
arr nombre : i8[16] = "R2-Piler";   // copia + NUL + ceros al resto
arr cero : u8[4];                   // sin inicializar (stack o .bss)

fn suma(v : i32[]) : i64 {          // slice: puntero + longitud
    var total : i64 = 0;
    var i : i64 = 0;
    while (i < v.len) {
        total += (v[i] as i64);
        i++;
    }
    return total;
}

fn primero(p : ptr i32) : i32 {
    return p[0];
}

fn exacto(v : i32[3]) : i64 {       // array por referencia: se pasa solo el puntero
    return (v[0] as i64) + v.len;   // .len es la constante 3
}

fn main() : i64 {
    arr local : i32[3] = {1, 2, 3};
    var p : ptr i32 = local;        // decay al primer elemento
    var q : ptr i32 = local + 1;    // decay en aritmética: &local[1]
    var n : u64 = local.len;        // constante flexible

    return suma(local) + (primero(local) as i64) + (p[1] as i64) + (n as i64)
        + (q[0] as i64) + exacto(local);
}
```

- **Declaración**: `arr ID : T[N] [= inicializador];` en locales y globales. `arr` es obligatorio: `var x : i32[5];` es error (`array declarations must use 'arr'`) y `arr x : i64;` también (`'arr' declarations must have an array type`). Un slice local (`arr x : i32[];`) es error (`slice types are only allowed as function parameters`).
- **Dimensión**: cada `[N]` debe ser una expresión entera constante y mayor que cero; una variable (`i32[n]`) es `array size must be a constant integer expression`, `0` es `array size must be greater than zero` y un tamaño total que no cabe es `array is too large`. En esta parte solo se admite **una** dimensión; `i32[2][3]` da `multidimensional arrays are not supported yet` (las matrices llegan en la parte 3).
- **Memoria**: los arrays locales viven en el stack; los globales en `.bss`. Un global con inicializador **no** se liga a `.data`: `.bss` queda a cero y `__r2_init` escribe los elementos con stores. Un global sin inicializador ya vale cero. Un array local sin inicializador queda indefinido, como un `var` sin init.
- **Inicializadores**: `{...}` debe ser **exhaustivo y exacto** (`array initializer must have exactly N elements (found M)`); no hay zero-fill implícito ni inicializadores anidados todavía (`nested array initializers are not supported yet`). Para `i8[]`/`u8[]` se admite un literal string: se copian los bytes más el terminador NUL y, en locales, el resto se rellena con ceros; si `len + 1 > N` es error (`string literal needs ... but the array has room for ...`) y en un array que no sea `i8`/`u8` también (`string initializer requires an i8 or u8 array`). Los elementos siguen las reglas de asignación (literal flexible o ensanche silencioso); un valor no convertible es error en el elemento. Un `{...}` sobre un tipo no array es error (`'{...}' initializer requires an array type`).
- **`.len`**: en un array concreto se pliega a una constante **flexible** (como `sizeof`), así que adopta el tipo del contexto si cabe (`var n : u64 = lista.len;`); sin contexto es `i64`. En un slice es un valor `i64` en runtime (la longitud oculta). Es **solo lectura**: `lista.len = 3;` es error (`left side of '=' must be a variable`). En un array/slice, un campo distinto de `len` es `type T has no field 'X'`; sobre un escalar, `.` sigue las reglas de enum (`'x' is not an enum type`).
- **Decay a puntero**: un array usado en contexto de puntero (`var p : ptr T = a;`, asignación, argumento de tipo `ptr T`, `return`, llamada `extern`, `$a`, y los operandos de `+`/`-` con un puntero o un entero) vale `ptr T` al elemento 0. No hay puntero-a-array ni copia por valor. `a[i]`, `a.len`, `sizeof(a)` y la conversión a slice se resuelven sobre el array **antes** de decaer. Un slice en contexto de puntero da su puntero de datos (también en `+`/`-`).
- **Índice**: `a[i]` es un *lvalue* exactamente como `p[i]`/`@(p + i)`, con el offset escalado por `sizeof(T)`. El índice se finaliza a su tipo por defecto; un no entero es `array indexing requires an integer index`. Si el índice es una constante conocida, se comprueba en compilación (`index 3 is out of bounds for an array of size 3`, también para negativos); un índice variable **no** se comprueba (UB, como en C). Se encadena por la izquierda (`a[i]`; las matrices de la parte 3 añadirán `m[i][j]`).
- **Aritmética**: en `+` y `-` el array (o el slice) decae a `ptr T` y la operación sigue las reglas de punteros: `a + i`, `i + a` y `a - i` dan `ptr T` con el offset escalado por `sizeof(T)`, y `a - b` (dos arrays del mismo tipo de elemento) da la distancia en **elementos** como `i64`. `a + b` es error (`cannot add two pointers`). El resto de operadores no acepta arrays: no se puede asignar un array entero (`a = b` → `cannot assign a value of type ...; arrays and slices must be modified element by element`), ni usar `+=`, `++`/`--`, `-a`, `a * 2`, comparaciones o como condición (`operator cannot be applied to a value of type ...` / `expression with resulting type ... is not valid as a condition`). Se modifica elemento a elemento: `a[i] = ...`, `a[i] += ...`, `a[i]++`.
- **Parámetros**: `fn f(v : T[])` recibe un slice (puntero + longitud); la llamada `f(mi_array)` calcula la longitud del array automáticamente, y `f(otro_slice)` propaga puntero y longitud. `fn f(x : T[N])` recibe un array de longitud **fija**: se pasa únicamente el puntero al primer elemento (no viaja ninguna longitud) y el array es modificable desde dentro, como en C; en la llamada el argumento debe ser un array de **exactamente** ese tipo (mismo elemento y misma longitud), así que cualquier otra cosa es error (`argument type ... does not match parameter type ...; an array parameter requires an array of exactly that length and element type`), incluidos un `ptr T`, un `T[]` o un `T[M]` con `M != N`. Dentro de la función `x.len` pliega a la constante `N`, `sizeof(x)` es `N * sizeof(T)` y `x[i]` con índice constante se comprueba igual que en un array local. Una función no puede devolver un array ni un slice (`functions cannot return an array or a slice`). Una función `extern` tampoco puede tomar slices (`extern functions cannot take slice parameters`), aunque sí arrays `T[N]`, que para C son un `ptr T`; para la longitud por separado se usa un parámetro aparte.
- **`sizeof`**: `sizeof(a)` es el total de bytes del array (`N * sizeof(T)`), no 8; `sizeof(a[0])` es el tamaño del elemento. El de un slice es el descriptor de 16 bytes. Como cualquier `sizeof`, el resultado es una constante flexible.
- **Tipos de elemento**: entero, `char`, enum, `newtype` o alias de esos. Un array de punteros aún no es expresable (`ptr i32[2]` se interpreta como puntero a array y se rechaza); llega con las matrices/estructuras.

---

## 4. Operadores: precedencia y asociatividad

De mayor a menor precedencia. Todos los binarios son asociativos por la izquierda excepto las asignaciones (`=`, `+=`, …), que lo son por la derecha.

| Prec | Operadores | Resultado |
|------|-----------|-----------|
| postfijo | `expr "[" expr "]"` | tipo del elemento (lvalue) |
| unario | `-` `!` `~` `@` `$` (prefijo), `sizeof` | `-`/`~` preservan tipo, `!` → `i64`, `@e` → tipo apuntado, `$e` → `ptr T`, `sizeof` → literal flexible (`i64`) |
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
| 1 | `=` `+=` `-=` `*=` `/=` `%=` `&=` `\|=` `^=` `<<=` `>>=` | tipo del LHS |

Los compuestos (`+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`) son tokens propios, no `+` seguido de `=`; `<<=`/`>>=` son tokens de tres caracteres. `x OP= e` es exactamente `x = x OP e` (misma precedencia de asignación y misma asociatividad por la derecha).

La indexación es postfija y se encadena por la izquierda (`p[i][j]` es `(p[i])[j]`).
El unario tiene la precedencia más alta tras la indexación y su operando es otro unario: `-x + y` es `(-x) + y`, y `- -x` / `@$x` están permitidos.
`sizeof` va a la misma altura que el unario, pero su operando no es una expresión cualquiera: admite un tipo o una expresión "cerrada" (una variable, una desreferencia `@p`, una indexación `p[i]` o un literal string), así que `sizeof(i64)`, `sizeof(x)`, `sizeof(@p)`, `sizeof(p[0])` y `sizeof("abc")` valen y `sizeof($x)` o `sizeof(1 + 2)` son error.
El cast está justo debajo del unario y por encima de todos los binarios (como en Rust: `unary > as > *`):
`-x as i64` es `(-x) as i64`, `1 as i64 + 2` es `(1 as i64) + 2`, `1 + 2 as u64` es `1 + (2 as u64)`.
Se encadena por la izquierda: `x as i64 as u64` es `(x as i64) as u64`. Los paréntesis fuerzan otro agrupamiento: `(1 + 2) as i64`.
Con punteros: `@p + 1` es `(@p) + 1` (suma al *valor* apuntado, no al puntero) y `$x as ptr i64` es `($x) as ptr i64`.

---

## 5. Expresiones

- Literales enteros, char y string; identificadores; llamadas `f(a, b)`; expresiones entre paréntesis.
- **Acceso a variante `Tipo.Variante`**: `Tipo` debe ser el nombre de un enum y `Variante` una variante existente; la expresión es una constante de compilación del tipo del enum (ver §3.1). El `.` se resuelve dentro del primario, así que `Dias.Lunes as u8` es `(Dias.Lunes) as u8`.
- **Cast `expr as type`** (cualquier tipo numérico, `char` = `i8`): conversión explícita que acepta cualquier operando no-`void` y devuelve exactamente el tipo escrito. Todas las combinaciones están permitidas (incluido el cast identidad `x as i64` con `x : i64`), pero el cast identidad emite un **warning** (`redundant cast to 'i64'; the value already has that type`); un cast que cambia el tipo nunca advierte. Entre 64 bits (`i64<->u64`) es reinterpretación de bits (wrap/módulo 2^64); hacia un tipo narrow es truncado al ancho; hacia 64 bits desde un narrow es extensión con el signo del origen (`i8`→`i64` extiende con signo, `u8`→`u64` con ceros). El cast no es asignable: `(x as i64) = 1` es error (`left side of '=' must be a variable`). Como cualquier expresión, puede aparecer en inits, args, `return` y condiciones.
- **Asignación `=`**: es una expresión. El lado izquierdo debe ser una variable; el derecho debe ser del mismo tipo o ensanchar a él en silencio (ver punto 3; si no, error `cannot assign X to a variable of type Y; use an explicit cast with 'as'`, reportado en el `=`). Devuelve **el valor asignado**, por lo que puede encadenarse (`x = y = 1`) y usarse como expresión, incluso como condición. Asignar una variable a sí misma (`x = x;`) emite un **warning** (`'x' is assigned to itself; this has no effect`).
- **Asignación compuesta `x OP= e`** (`+= -= *= /= %= &= |= ^= <<= >>=`): es una expresión que lee `x` una sola vez, calcula `x OP e` y guarda el resultado en `x`, devolviéndolo; puede encadenarse (`a += b += 1` es `a += (b += 1)`). El LHS debe ser una variable (si no, error `left side of compound assignment must be a variable`, reportado en el operador). Para `+= -= *= /= %= &= |= ^=`, el RHS se adapta al tipo del LHS con las reglas de asignación (mismo tipo, literal flexible o ensanche implícito); si no, error `cannot assign X to a variable of type Y; use an explicit cast with 'as'` en el operador, igual que `x = x OP e`. `<<=` y `>>=` siguen las reglas del desplazamiento binario: el RHS puede ser cualquier numérico y el resultado tiene el tipo del LHS. `x /= 0` y `x %= 0` reportan `division by zero` en el `0`.
- **`&&` y `||`**: short-circuit. Si el resultado queda decidido por el operando izquierdo, el derecho no se evalúa. Un operando constante emite un **warning** (`constant operand of '&&' is always true/false`), también a través de un cast (`(1 as i64) && x` advierte). Un puntero y `NULL` son operandos válidos (y `NULL` advierte); un literal string no.
- **`$x` (dirección de)**: `NODE_ADDR`, unario, sobre una **variable escalar**, una desreferencia o una indexación (`$@p` da el propio `p`; `$p[i]` da `p + i`). Da `ptr T` con el tipo del operando. Sobre un array o slice en crudo es error: el array suelto ya decae a puntero a su primer elemento, usa el array directamente.
- **`@p` (desreferencia)**: `NODE_DEREF`, unario, sobre un `ptr T` no-`void`. Es un **lvalue**: vale como valor (`var y : i64 = @p;`) y como destino (`@p = 1;`, `@p++;`, `@p += 2;`). `@` y `$` son prefijos de la máxima precedencia, así que `@p + 1` es `(@p) + 1` (y `$x as ptr i64` es `($x) as ptr i64`).
- **`p[i]` (indexación)**: `NODE_INDEX`, postfijo, sobre un `ptr T` no-`void` y un índice entero. Es azúcar exacta de `@(p + i)` y produce el mismo *lvalue* (`p[i] = 1;`, `p[i]++;`, `p[i] += 2;`, `var y : i64 = p[i];`, `$p[i]`). Se encadena: `pp[i][j]` es `(pp[i])[j]`. Mismos errores que `@`: `ptr void` (`cannot index 'ptr void'; cast it to a typed pointer first`), no puntero (`cannot index a value of type T`) e índice no entero (`pointer indexing requires an integer index`). El warning de `NULL` conocido también aplica. Un literal string no es indexable (`"abc"[i]` es error). No hay comprobación de límites.
- **`sizeof`**: acepta un **tipo** (`sizeof(i64)`, `sizeof(ptr i32)`, `sizeof(MiEnum)`, `sizeof(Alias)`) o una **expresión** que sea una variable, una desreferencia, una indexación o un literal string (`sizeof(x)`, `sizeof(@p)`, `sizeof(p[0])`, `sizeof("abc")`). Cualquier otra cosa (`sizeof(f())`, `sizeof(1 + 2)`, `sizeof(sizeof(x))`) es error (`operand of sizeof must be a variable, dereference, index, string literal or type`), igual que `sizeof(void)`. El resultado es una constante plegada en compilación y **flexible como cualquier literal**: adopta el tipo que pida el contexto si cabe (`var t : i64 = sizeof(i32)`, `var u : u64 = sizeof(i32)`, `var a : i8 = 1 + sizeof(i8)`), así que no hay que castear. Un literal string mide sus bytes más el terminador (`sizeof("abc")` = 4).
- **Aritmética y comparación de punteros**: ver §3 (bloque "Punteros"). `p + n`/`p - n` dan `ptr T`, `p - q` da la distancia en elementos (`i64`) y las comparaciones solo valen entre punteros del mismo tipo o contra `NULL`. Los literales string entran aquí como `ptr i8`; restar dos literales string advierte.

---

## 6. Statements

- Declaración de variable: `var ID : type (= expr)? ;`
- Expresión: `expr ;`. Una sentencia de expresión cuyo resultado se descarta y que no tiene efectos — sin llamada, asignación, asignación compuesta ni `++`/`--` en toda la expresión — emite un **warning** (`expression has no effect; its result is discarded`): `x + 1;`, `@p;`, `p[0];`, `x as i64;`.
- **`i++;` / `i--;`**: solo postfijo y solo como sentencia (también vale como update de un `for`). Se representa con nodos propios `NODE_INC`/`NODE_DEC`; el operando debe ser una variable y el IR emite carga, suma/resta de `1` del tipo del operando y store. `++i` es error (`'++' cannot be used as a prefix; use the postfix form as a statement`) y usarlo dentro de una expresión también (`x = i++;`, `return i++;` → `'++' is only valid as a standalone statement on a variable`).
- Bloque: `{ statement* }`
- `if (cond) body { elif (cond) body } [ else body ]` — el cuerpo es cualquier statement, normalmente un bloque.
- `while (cond) body`
- `for (init; cond; updt) body`:
  - `init`: declaración `var` (con su `;`), expresión, o vacía.
  - `cond`: expresión o vacía (vacía = verdadero: `for(;;)` es un bucle infinito válido).
  - `updt`: expresión o vacía.
- `return expr? ;` — en una función `void`: `return;` (devolver un valor es error: `function returning void cannot return a value`). En una función `T`: `return expr;` con tipo `T` o que ensanche a `T` en silencio; si no, error (`cannot return X from a function returning Y; use an explicit cast with 'as'`); sin valor, error (`missing return value of type T`).

---

## 7. Scopes y variables

- Cada bloque `{}` abre un scope. Los cuerpos de `if`/`while`/`for` sin bloque abren un scope propio.
- Redeclarar un nombre **en el mismo scope** es error. Shadowing de scopes exteriores está permitido.
- Una local no puede referenciarse antes de su declaración (el init se chequea antes de declarar la variable).
- **Local sin init**: permitida. Si se lee sin haberse asignado, sema emite un **warning** (no error). Su valor en runtime es **indefinido** (lo que haya en el stack).
- El análisis de "asignada" es conservador y sensible al flujo: una variable cuenta como asignada si tiene init, si es parámetro, si se le asignó antes en el mismo flujo incondicional, o si **todas** las ramas de un `if`/`elif`/`else` la asignan (un `if` sin `else` no cuenta; lo asignado dentro de un `while`/`for` tampoco escapa del cuerpo). En otro caso la lectura emite el warning (`'x' is used without being assigned`). El cuerpo de un `if` con condición siempre-true se trata como incondicional.
- **Local no leída**: una local que nunca se lee emite un **warning** (`unused variable 'x'`) al cerrar su scope. Escribirla no cuenta como leerla: `var x : i64 = 1; x = 2;` también advierte. Leerla en cualquier expresión (incluidas una condición, un `return`, `$x`, `@p`, el destino de una asignación compuesta o un `++`/`--`) cuenta. La advertencia se reporta en el nombre declarado.
- **Valor asignado no usado**: si a una local o parámetro se le asigna un valor que una escritura posterior del mismo flujo sobrescribe sin que se haya leído antes, sema emite un **warning** (`value assigned to 'x' is not used`) en el punto del init o de la asignación: `var x : i32 = 7; x = 6; return x;` advierte sobre el `7`. Una lectura de la variable anula el seguimiento pendiente; entrar en una región condicional (cuerpo de un `if` con condición no constante, `elif`/`else`, cuerpo de `while`/`for`, RHS de `&&`/`||`) o volver de una bifurcación lo descarta, de modo que el análisis solo puede omitir avisos, nunca inventarlos. No se aplica a globales ni a variables cuya dirección se toma (`$x`). Si la variable nunca se lee, se emite solo el warning de local no leída.
- **`NULL` conocido**: análisis conservador y sensible al flujo, análogo al de "asignada": un puntero es conocido `NULL` si su init o su última asignación incondicional fue `NULL` y todos los caminos vivos llegan con `NULL` (un `if`/`elif`/`else` lo conserva solo si **todas** las ramas asignan `NULL`; un `if` sin `else`, un `while`/`for` o el RHS de `&&`/`||` no lo conservan). Cualquier otra escritura (incluida una copia `p = q`) lo pierde. Solo alimenta el warning de desreferencia nula (§3); nunca es un error.

---

## 8. Funciones

- Solo top-level. Declarar una función dentro de otra es error.
- `fn nombre(params)? (: ret_type)? block` — sin `: tipo` el retorno es `void`.
- Parámetros: `ID : type`, por valor. Siempre se consideran asignados. Un parámetro que nunca se lee emite un **warning** (`unused parameter 'a'`), con la misma regla que las locales (§7).
- Forward references y recursión permitidas: las funciones se declaran todas antes de chequear los cuerpos.
- Las llamadas comprueban arity y, parámetro a parámetro, tipo `T` o ensanche silencioso a `T` (los literales adoptan el tipo del parámetro si caben); si no, error (`argument type X does not match parameter type Y; use an explicit cast with 'as'`).
- **Return obligatorio en todos los caminos** para funciones no-`void` (chequeo conservador):
  - Un `return` garantiza.
  - Un bloque garantiza si alguno de sus statements garantiza.
  - Un `if` garantiza solo si tiene `else` final y todas las ramas (then/elif/else) garantizan.
  - `while`/`for` garantizan solo si su condición es constante verdadera (`while (1)`, `for (;;)`): sin `break`, el bucle solo sale con `return`. Con condición no constante nunca garantizan.

### 8.1 `extern` (interoperabilidad con C)

- `extern fn nombre(params)? (: ret_type)? ;` declara una función definida fuera del programa (típicamente libc), y `extern var nombre : type ;` una variable global definida fuera. Ambas son solo top-level.
- Nunca llevan cuerpo ni inicializador: `extern var x : i64 = 1;` es error de parser (`extern variables cannot have an initializer`).
- sema comprueba arity y tipos en cada llamada igual que con una función propia, pero no exige cuerpo. Una `extern var` cuenta como siempre asignada (no dispara el warning de lectura sin asignar).
- El nombre no se mangla ni lleva prefijo: el assembly emite `call puts` / `movl optind(%rip), %eax`, y la resolución queda para el `gcc` final. La dirección `$optind` también usa el símbolo plano.
- Antes de cada llamada a una función `extern` el backend emite `xorl %eax, %eax` (ABI SysV: `%al` = número de registros vectoriales para funciones variádicas; el compilador lo emite siempre, con o sin variádicos).
- No puede llamarse `main`: `'main' cannot be declared extern` (`extern fn`) y `'main' must be a function` (`extern var`). Los nombres con prefijo `__r2_` siguen reservados.
- `extern` no cambia la representación: los parámetros y el retorno siguen la ABI entera SysV AMD64 (hasta 6 argumentos en registros, el resto en la pila) y los punteros/`ptr void` se pasan como punteros C.

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
- El init de una global de tipo puntero debe ser una **constante de enlace**: `NULL` (o un cast de `NULL`), un literal string (a `ptr i8`) o la dirección de una global (`$g`). Un valor es error: `var p : ptr i64 = q;` (`global pointer initializers only support NULL, a string literal or the address of a global`); `$@p` tampoco vale, porque depende de un valor. En `-O2`/`-O3` estos inits se ligan estáticamente en `.data`; en `-O0`/`-O1` los escribe `__r2_init` en runtime.

---

## 10. Condiciones

- Toda condición (`if`, `elif`, `while`, `for`) es una expresión de tipo no-`void` (un cast vale como condición: `if (x as i64)` es válido si `x` no es `void`).
- Cualquier valor **distinto de cero es verdadero**; cero es falso.
- Un **puntero** (y `NULL`) es una condición válida: `if (p)` es falso si `p` es `NULL`. `NULL` es una constante, así que advierte (`condition is always false`). Un **literal string** no es una condición válida (`expression with resulting type string is not valid as a condition`).
- La cond vacía de `for` cuenta como verdadera.
- Una condición constante (literal o expresión plegada a literal, también tras un cast: `if (1)`, `if (0)`, `if (1 == 1)`, `while (0)`) emite un **warning** (`condition is always true` / `condition is always false`). La cond vacía de `for` no advierte.

---

## 11. Entry point y salida

- El entry point es exactamente `fn main() : i64`, sin parámetros. Su valor de retorno es el exit code del programa.
- Los inits dinámicos de las globales se ejecutan antes de `main` (ver la sección 9).
- El prefijo `__r2_` está reservado a nivel top-level y lo rechaza el compilador (`identifiers starting with '__r2_' are reserved`).
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

El **wrap** aritmético NO es indefinido: está definido como complemento a 2 (y módulo 2^64 para `u64`). Tampoco lo son los shifts con cuenta menor que 64: se calculan a 64 bits sobre el valor extendido y el resultado se trunca al tipo del operando izquierdo (p. ej. `u8 << 9` vale 0 y `i8 >> 9` rellena con el signo).

Plegado de constantes: una subexpresión hecha solo de literales se evalúa en compilación con esa misma semántica de wrap, siempre a 64 bits (ver "Ancho del plegado"), incluida la negación: `-5u` es la constante `u64` exacta, sin warning. Los literales detrás de un cast explícito también pliegan, truncando primero al tipo del cast (`-(300 as u8)` es la constante `212`). El divisor cero conocido (literal, plegado o tras un cast) es error de compilación (`division by zero`; `division by zero in constant expression` si ambos operandos son literales). No se pliegan y se dejan al hardware: `INT64_MIN / -1` (o `%`), el mínimo de un narrow dividido por `-1` y los shifts con cuenta `>= 64`.

---

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
- **Globales**: forwarding de lecturas dentro del mismo bloque y eliminación de stores muertos.
- **Coalescing**: fusiona una operación con la copia que la sigue.
- **Inits estáticos**: una global inicializada con una constante pasa a `.data` (sin código de arranque).

Solo en `-O3`:

- **CSE**: reutiliza `*`, `/` y `%` repetidos con el mismo divisor no constante.
- **DCE global**: eliminación de código muerto con liveness global, no solo local.
- **Inlining**: inlinea pequeñas funciones hoja.
- **`select`**: if-conversion de `if (c) x = a; else x = b;` con ramas simples (no aparece en `--dump-ir`; sí en `--dump-opt`).

Las declaraciones `extern` quedan fuera de las optimizaciones: una función `extern` no tiene cuerpo (no se optimiza ni se inlinia) y una `extern var` nunca se considera constante, porque cualquier llamada a C puede modificarla.

El codegen aplica además optimizaciones locales (operaciones read-modify-write, fusión de comparación con salto, tail calls) en todos los niveles; `-O0` fuerza que los valores vivan en pila.

---

## 14. Gramática (EBNF)

```
program        = { fn_decl | var_decl | enum_decl | type_decl | newtype_decl | extern_decl } ;
extern_decl    = "extern" ( "fn" IDENT "(" [ params ] ")" [ ":" ret_type ] ";"
                          | "var" IDENT ":" type ";" ) ;
fn_decl        = "fn" IDENT "(" [ params ] ")" [ ":" ret_type ] block ;
params         = param { "," param } ;
param          = IDENT ":" type ;
ret_type       = "void" | type ;
type           = "i8" | "u8" | "i16" | "u16" | "i32" | "u32" | "i64" | "u64" | "char"
               | "ptr" type | IDENT ;
enum_decl      = "enum" IDENT [ ":" type ] "{" enum_val { "," enum_val } [ "," ] "}" [ ";" ] ;
enum_val       = IDENT [ "=" expr ] ;
type_decl      = "type" IDENT "=" type ";" ;
newtype_decl   = "newtype" IDENT "=" type ";" ;
block          = "{" { statement } "}" ;
statement      = var_decl | block | if_stmt | while_stmt | for_stmt
               | "return" [ expr ] ";"
               | inc_dec_stmt
               | expr ";" ;
inc_dec_stmt   = inc_dec_target ( "++" | "--" ) ";" ;
inc_dec_target = IDENT | "@" unary | postfix ;
var_decl       = "var" IDENT ":" type [ "=" expr ] ";" ;
if_stmt        = "if" "(" expr ")" statement
                 { "elif" "(" expr ")" statement }
                 [ "else" statement ] ;
while_stmt     = "while" "(" expr ")" statement ;
for_stmt       = "for" "(" for_init for_cond for_updt ")" statement ;
for_init       = var_decl | [ expr ] ";" ;
for_cond       = [ expr ] ";" ;
for_updt       = [ expr | inc_dec_target ( "++" | "--" ) ] ;
expr           = assignment ;
assignment     = logic_or [ ( "=" | "+=" | "-=" | "*=" | "/=" | "%="
                            | "&=" | "|=" | "^=" | "<<=" | ">>=" ) assignment ] ;
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
               | postfix ;
postfix        = primary { "[" expr "]" } ;
sizeof_arg     = "@" unary | postfix ;
primary        = INT_LIT | CHAR_LIT | STRING_LIT | "NULL"
               | IDENT | IDENT "(" [ args ] ")"
               | IDENT "." IDENT
               | "(" expr ")" ;
args           = expr { "," expr } ;
```

Notas: `var_decl` dentro de `for_init` incluye su propio `;`. Los literales se definen en la sección 2. `char` es alias de `i8` en cualquier posición de `type`. Por leniencia el parser acepta una coma final en parámetros (`fn f(a : i64,)`) y argumentos (`f(a,)`), aunque la EBNF no la muestra. El cast es postfijo y asociativo por la izquierda (`x as i64 as u64` = `(x as i64) as u64`); el tipo tras `as` nunca puede ser `void`. En `type`, un `IDENT` se resuelve contra los enums y las declaraciones `type`/`newtype` (también las declaradas más tarde); un nombre no declarado es `unknown type 'Foo'`. `enum_decl` exige al menos una variante (el parser acepta `{}` para que sema emita `enum 'E' must have at least one variant`); la coma final y el `;` tras `}` son opcionales. El valor de una variante debe ser un literal, `-literal`, `~literal` o un cast de esas formas; sema rechaza cualquier otra expresión. En `IDENT "." IDENT`, el identificador izquierdo debe ser un enum o un tipo cuyo tipo representado sea un enum (alias o newtype); el valor resultante tiene el tipo nominal escrito. La indexación es un sufijo postfijo que se encadena (`p[i][j]` = `(p[i])[j]`) y se aplica antes que el unario (`-p[0]` = `-(p[0])`). `@` y `$` son operadores prefijos del nivel `unary`, así que `@p + 1` es `(@p) + 1`; `sizeof` también es prefijo y su operando está restringido a `type`, `IDENT`, `@` sobre una expresión unaria, una indexación o un literal string (cualquier otra cosa es `operand of sizeof must be a variable, dereference, index, string literal or type`). La recursión de `ptr` en `type` también está limitada por la profundidad del parser (`type nested too deeply`). El objetivo de `++`/`--` puede ser una variable, una desreferencia (`@p++`) o una indexación (`p[i]++`), nunca una dirección (`$x++`).

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
| `-a` / `--arch ARCH` | Arquitectura: `x86-64` (alias `x86_64`, `amd64`), `arm`/`aarch64`, `riscv`/`riscv64`. Solo x86-64 está implementado |
| `-e` / `--execute` | Modo intérprete (no implementado) |
| `-v` / `--version` | Muestra la versión |
| `-h` / `--help` | Muestra la ayuda |

Nota: sin flags de dump, `r2p` compila y escribe el assembly a `<fuente>.s` (o a la ruta de `-o`; `-` es stdout). `-a arm`, `-a riscv` y `-e` abortan en codegen con `Error: <backend> backend is not implemented` (`ARM`, `RISC-V` o `interpreter`). Los modos de dump no pasan por codegen, así que pueden inspeccionar un programa aunque la arquitectura elegida no tenga backend. Los punteros y los literales string están completos de punta a punta: la aritmética (`p ± n`, `p - q` en elementos, `p++`/`p--`, `p += n`), los casts entre punteros, `@p`, `$x`, `NULL` y los literales string se analizan y se generan a assembly (los strings van a una piscina deduplicada en `.rodata`; las locales cuya dirección se toma reciben slot de pila; los inits globales de puntero que son constantes de enlace — `NULL`, string o `$g` — se ligan estáticamente en `.data`). Si se pasan varios ficheros, solo se compila el primero y se avisa por stderr.

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

fn main () : i64
{
	var f : i64 = fact(start);
	if (f == 120) {
		return 0;
	}
	return 1;
}
```

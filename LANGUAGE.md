# R2-Lang — Especificación del lenguaje

Especificación completa de R2-Lang, el lenguaje que compila R2-Piler.

Estado: lexer, parser, sema e IR implementados y testeados. Codegen: el andamiaje existe (CLI, tabla de backends, buffer de salida) pero el backend x86-64 es un stub que aún no emite assembly (`arm`, `riscv` y el intérprete se rechazan con "backend is not implemented"). `optimize_ir` pendiente.

---

## 1. Panorama

R2-Lang es un lenguaje imperativo, estática y fuertemente tipado, sin conversiones implícitas
(solo casts explícitos con `expr as type`).

- Tipos primitivos: `i8`, `u8`, `i16`, `u16`, `i32`, `u32`, `i64`, `u64` y `void`. `char` es un alias de `i8` aceptado solo en el frontend (lexer/parser): en dumps, símbolos y mensajes siempre aparece `i8`.
- Funciones solo a nivel top-level, con parámetros por valor.
- Scopes de bloque con shadowing entre scopes.
- Entry point: `fn main() : i64`.
- El compilador emitirá assembly x86-64 (no binarios: el binario final se produce ensamblando). **Hoy el backend es un stub**: una compilación sin flags de dump escribe un `out.s` vacío.

Pipeline del compilador:

```
lexer -> parser -> sema -> IR -> optimize_ir -> codegen (assembly)
```

Un error en cualquier fase aborta la compilación antes de la siguiente. Los warnings no abortan.

---

## 2. Estructura léxica

### 2.1 Comentarios

`//` hasta el fin de línea. No existen comentarios de bloque. Solo `\n` termina un comentario: un `\r` solitario no es fin de línea y el comentario lo consume como un carácter más.

### 2.2 Identificadores y keywords

Identificadores: `[a-zA-Z_][a-zA-Z0-9_]*`.

Keywords (una palabra clave nunca puede usarse como identificador):

```
fn  if    for    i8   u8   i16  u16  i32  u32
i64  u64   var   char  elif  else  void
while  return  as
```

`as` es keyword desde los casts explícitos: no puede usarse como identificador
(`as_`, `asdf` siguen siendo identificadores válidos).

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
- **Hoy:** se lexan y parsean, pero el type checker los rechaza. Su semántica se define cuando exista punteros.

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
| `void` | — | Solo como tipo de retorno de función. |

`char` es un alias de `i8` aceptado solo donde se escribe un tipo (`var x : char`, `fn f(a : char) : char`, `x as char`). No existe como tipo propio: en `--dump-ast`, `--dump-symbols`, `--dump-ir` y en todos los mensajes aparece `i8`.

Reglas de tipos (estrictas; el único implícito es el que no puede perder valor):

- **Literales flexibles**: un literal (o `-literal`, o una expresión hecha solo de literales) adopta en silencio el tipo que le pide el contexto si su valor cabe en él: `var u : u64 = 1`, `take_i64('a')`, `c > 5` (el `5` se vuelve `i8`), `var v : u64 = 9223372036854775808` (sin sufijo, cabe en `u64`). Las subexpresiones puramente literales se **pliegan en compilación** con semántica de wrap, y el chequeo de rango se hace sobre el valor plegado: `var w : char = 100 * 100` es error (`literal 10000 does not fit in type i8`, reportado en el operador). Los casts de literales también pliegan (el valor se trunca al tipo del cast antes de plegar): `-(5 as u64)` es la constante `u64` exacta, `(300 as u8) > 200` pliega a `0`. No se pliega (se deja al hardware): división/módulo por cero, `INT64_MIN / -1` y shifts con cuenta `>= 64`.
- **Ancho del plegado**: toda subexpresión hecha solo de literales (untyped) pliega con wrap a **64 bits**, sea cual sea el sabor del literal: `var x : i64 = 'a' + 'b';` da **195**. Solo los pares de operandos con tipo concreto narrow (p. ej. dos casts) pliegan con truncado al ancho de ese tipo (`(250 as u8) + 10` pliega a `4`, lo mismo que haría `add.u8` en runtime). Consecuencia: una constante que no cabe en el tipo destino es error aunque "envolvería" bien en un tipo más ancho (`var a : i8 = 1 << 7;` → `literal 128 does not fit in type i8`).
- **Locación de los errores de rango**: un literal suelto se reporta **en el literal** (`c + 300` marca el `300`); un `-literal` marca el `-literal` **completo**; una subexpresión plegada se reporta **en el operador** (el nodo plegado hereda la posición del operador), y un init en el nombre declarado.
- **Shifts con izquierda untyped**: si la cuenta es un literal, la constante plegada queda flexible y adopta el tipo del contexto (`var v : u64 = 1 << 2;` vale). Si la cuenta **no** es un literal, la izquierda finaliza a su tipo por defecto (`1 << x` es `i64`, `'a' << x` es `i8`) y el shift ya no adopta el contexto: `var a : i8 = 1 << x;` es un error de estrechamiento, igual que `var a : i8 = 1 << 7;`. Migración: `var v : u64 = 1 << x;` → `var v : u64 = (1 << x) as u64;` (la misma regla que ya aplicaba a `var v : u64 = 5 + x;`).
- **Literal fuera de rango**: error (`literal 300 does not fit in type i8; use an explicit cast with 'as' to reinterpret the bits`). Si además el destino es con signo y el valor supera `INT64_MAX`, el mensaje añade que el sufijo `u` lo convierte en literal `u64`. Un literal negativo hacia un destino sin signo es error, también dentro de binarios (`var u : u64 = -5;`, `5u + -1`); los mensajes siempre usan el nombre público del tipo (`u64`, nunca el interno). Un literal fuera de rango produce exactamente un diagnóstico, sin cascadas.
- **Literal `u` usado como tipo con signo**: solo un **warning** (`unsigned literal used as signed type i64; remove the 'u' suffix`); compila igual.
- **Ensanchar sin cambiar el valor es silencioso**: mismo signo a mayor tamaño (`i8`→`i64`, `u8`→`u32`), o sin signo a con signo mayor (`u8`→`i64`). Vale en binarios, init, asignación, args y `return`, sin emitir código (los regs ya guardan el valor extendido).
- **Todo lo demás entre tipos distintos es error** con sufijo `'as'` en el mensaje: estrechar (`i64`→`i8`), cambiar de signo (`i8`→`u64`, `u64`→`i64`), o mezclar dos variables de tipos distintos en un binario (`type mismatch: i64 vs u64 (use 'as' to convert explicitly)`).
- Operadores aritméticos y bitwise (`+ - * / % & | ^`): ambos operandos del **mismo tipo** (tras lo anterior), resultado del tipo de los operandos.
- Shifts (`<< >>`): el resultado es el tipo del operando **izquierdo**; el derecho puede ser cualquier numérico (`a << b` con `a : i64`, `b : u64` vale).
- Comparaciones (`< > <= >= == !=`): ambos operandos del **mismo tipo** (un literal flexible adopta el del otro: `c > 5` con `c : char` compara en `i8`); dos variables de tipos distintos son error. Nunca `void`. Resultado siempre `i64` (0 o 1).
- Lógicos binarios (`&&` `||`): aceptan operandos de **cualquier tipo no-`void` sin conversión**; resultado siempre `i64`.
- `-` y `~` unarios: preservan el tipo del operando. Negar una variable sin signo compila pero emite un **warning** (`negating an unsigned value; the result wraps`); negar un literal es silencioso (se pliega al valor exacto, también a través de un cast). `!` unario: acepta cualquier tipo no-`void`, resultado `i64`.
- **Cast explícito `expr as type`** (`type` = cualquier numérico, incluido `char` como alias de `i8`): acepta cualquier operando no-`void` (incluido el resultado de otro cast) y devuelve exactamente el tipo escrito. Reinterpreta/trunca bits en silencio. `as void` no existe (error de parser: `expected type`). Castear un `void` (p. ej. `f() as i64` con `f : void`) es error de sema (`cannot cast a value of type void`). Castear un operando con error (no declarado, string, etc.) no añade un segundo error: el cast propaga `error`.
- `void` no puede aparecer en expresiones (ni en condiciones, ni como operando, ni como init, ni como origen/destino de cast).

---

## 4. Operadores: precedencia y asociatividad

De mayor a menor precedencia. Todos los binarios son asociativos por la izquierda excepto `=` (por la derecha).

| Prec | Operadores | Resultado |
|------|-----------|-----------|
| unario | `-` `!` `~` (prefijo) | `-`/`~` preservan tipo, `!` → `i64` |
| cast | `expr as type` (postfijo, asociativo por la izquierda) | el `type` escrito (cualquier numérico) |
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
| 1 | `=` | tipo del LHS |

El unario tiene la precedencia más alta y se aplica sobre un primario: `-x + y` es `(-x) + y`. `- -x` está permitido.
El cast está justo debajo del unario y por encima de todos los binarios (como en Rust: `unary > as > *`):
`-x as i64` es `(-x) as i64`, `1 as i64 + 2` es `(1 as i64) + 2`, `1 + 2 as u64` es `1 + (2 as u64)`.
Se encadena por la izquierda: `x as i64 as u64` es `(x as i64) as u64`. Los paréntesis fuerzan otro agrupamiento: `(1 + 2) as i64`.

---

## 5. Expresiones

- Literales enteros, char y string; identificadores; llamadas `f(a, b)`; expresiones entre paréntesis.
- **Cast `expr as type`** (cualquier tipo numérico, `char` = `i8`): conversión explícita que acepta cualquier operando no-`void` y devuelve exactamente el tipo escrito. Todas las combinaciones están permitidas (incluido el cast identidad `x as i64` con `x : i64`). Entre 64 bits (`i64<->u64`) es reinterpretación de bits (wrap/módulo 2^64); hacia un tipo narrow es truncado al ancho + extendido (con signo si el destino lo tiene, con ceros si no); hacia 64 bits desde un narrow es no-op (el valor ya vive extendido en su reg). El cast no es asignable: `(x as i64) = 1` es error (`left side of '=' must be a variable`). Como cualquier expresión, puede aparecer en inits, args, `return` y condiciones.
- **Asignación `=`**: es una expresión. El lado izquierdo debe ser una variable; el derecho debe ser del mismo tipo o ensanchar a él en silencio (ver §3; si no, error `cannot assign X to a variable of type Y; use an explicit cast with 'as'`, reportado en el `=`). Devuelve **el valor asignado**, por lo que puede encadenarse (`x = y = 1`) y usarse como expresión, incluso como condición.
- **`&&` y `||`**: short-circuit. Si el resultado queda decidido por el operando izquierdo, el derecho no se evalúa. Compilan a una serie de saltos, no a operaciones aritméticas. Un operando constante emite un **warning** (`constant operand of '&&' is always true/false`), también a través de un cast (`(1 as i64) && x` advierte).

---

## 6. Statements

- Declaración de variable: `var ID : type (= expr)? ;`
- Expresión: `expr ;`
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
- El análisis de "asignada" es conservador e insensible al flujo: una variable se marca como asignada si tiene init, si es parámetro, o si aparece a la izquierda de una `=` en orden fuente. Una asignación en un path ya visitado silencia el warning aunque el path no se ejecute siempre.

---

## 8. Funciones

- Solo top-level. Declarar una función dentro de otra es error.
- `fn nombre(params)? (: ret_type)? block` — sin `: tipo` el retorno es `void`.
- Parámetros: `ID : type`, por valor. Siempre se consideran asignados.
- Forward references y recursión permitidas: las funciones se declaran todas antes de chequear los cuerpos.
- Las llamadas comprueban arity y, parámetro a parámetro, tipo `T` o ensanche silencioso a `T` (los literales adoptan el tipo del parámetro si caben); si no, error (`argument type X does not match parameter type Y; use an explicit cast with 'as'`).
- **Return obligatorio en todos los caminos** para funciones no-`void` (chequeo conservador):
  - Un `return` garantiza.
  - Un bloque garantiza si alguno de sus statements garantiza.
  - Un `if` garantiza solo si tiene `else` final y todas las ramas (then/elif/else) garantizan.
  - `while`/`for` nunca garantizan: un `while (1) { return x; }` sigue reportando "may reach the end without returning".

---

## 9. Globals

- `var ID : type (= expr)? ;` a nivel top-level. Otra cosa a nivel top-level es error (el parser acepta cualquier statement suelto como test de parser puro, pero sema lo rechaza: `only variable and function declarations are allowed at the top level`).
- Todas se declaran antes de chequear los inits: las **forward references están permitidas**.
- Referencias a variables globales futuras permitido, ej: `var x:i64 = y; var y:i64 = 73` es válido
- Una global **sin init vale 0** en runtime (vive en `.bss`) y su lectura **nunca advierte**; el warning de "used without being assigned" aplica solo a locales sin asignar.
- **Auto-referencia** (`var x : i64 = x;`): error.
- **Cadenas circulares** entre inits (`var y : i64 = x; var x : i64 = y;`): error. Al chequear el init de una global, si el init referencia a otra global cuyo init aún no se chequeó, ese init se chequea en ese momento (recursivamente); referenciar una global cuyo init está a medio chequear es el ciclo, y se reporta en el ref que lo cierra (un error por ciclo).
- Solo se siguen **referencias directas** a globals en los inits. Los ciclos a través de llamadas a funciones dentro de inits no se detectan.
- Las referencias a través de casts cuentan igual que las directas: `var y : u64 = x as u64;` fuerza el init de `x` antes que el de `y`, y `var x : i64 = y as i64; var y : i64 = x as i64;` es circular. Lo mismo para escrituras (`var y : i64 = (x = 1 as i64);` cuenta como referencia a `x`).
- Los inits pueden llamar funciones; esas llamadas se ejecutan en orden fuente durante el arranque.

---

## 10. Condiciones

- Toda condición (`if`, `elif`, `while`, `for`) es una expresión de tipo no-`void` (un cast vale como condición: `if (x as i64)` es válido si `x` no es `void`).
- Cualquier valor **distinto de cero es verdadero**; cero es falso.
- La cond vacía de `for` cuenta como verdadera.

---

## 11. Entry point y salida

- El entry point es exactamente `fn main() : i64`, sin parámetros. Su valor de retorno es el exit code del programa.
- Si el programa no define `main`: se genera assembly igualmente.
- La salida del compilador será **assembly x86-64**, no un binario. El binario final se produce ensamblando (p. ej. `gcc out.s`). Hoy el backend es un stub y el archivo sale vacío.

---

## 12. Comportamiento indefinido

No se añade ninguna lógica para evitarlo; el resultado es lo que haga el hardware:

- Shifts con cuenta `>= 64`.
- División o módulo por cero: fault del hardware (`SIGFPE`).
- Leer una local sin asignar: valor indefinido.

El **wrap** aritmético NO es indefinido: está definido como complemento a 2 (y módulo 2^64 para `u64`).

Plegado de constantes: una subexpresión hecha solo de literales se evalúa en compilación con esa misma semántica de wrap, siempre a 64 bits (ver §3, "Ancho del plegado"), incluida la negación: `-5u` es la constante `u64` exacta, sin warning. Los literales detrás de un cast explícito también pliegan, truncando primero al tipo del cast (`-(300 as u8)` es la constante `212`). No se pliega y se deja al hardware: división/módulo por cero, `INT64_MIN / -1` (o `%`) y shifts con cuenta `>= 64`.

---

## 13. IR y optimizaciones

- La IR está implementada y testeada (`--dump-ir`, suite `tests/ir/`). Toda la AST válida se baja a IR, incluido el código inalcanzable tras un `return`.
- La IR se genera solo si no hay errores en las fases anteriores; los warnings no la bloquean.
- `optimize_ir` es una pasada posterior (pendiente) que hará poda de código muerto, eliminación de código inalcanzable y optimizaciones.

---

## 14. Gramática (EBNF)

```
program        = { fn_decl | var_decl } ;
fn_decl        = "fn" IDENT "(" [ params ] ")" [ ":" ret_type ] block ;
params         = param { "," param } ;
param          = IDENT ":" type ;
ret_type       = "void" | type ;
type           = "i8" | "u8" | "i16" | "u16" | "i32" | "u32" | "i64" | "u64" | "char" ;
block          = "{" { statement } "}" ;
statement      = var_decl | block | if_stmt | while_stmt | for_stmt
               | "return" [ expr ] ";"
               | expr ";" ;
var_decl       = "var" IDENT ":" type [ "=" expr ] ";" ;
if_stmt        = "if" "(" expr ")" statement
                 { "elif" "(" expr ")" statement }
                 [ "else" statement ] ;
while_stmt     = "while" "(" expr ")" statement ;
for_stmt       = "for" "(" for_init for_cond for_updt ")" statement ;
for_init       = var_decl | [ expr ] ";" ;
for_cond       = [ expr ] ";" ;
for_updt       = [ expr ] ;
expr           = assignment ;
assignment     = logic_or [ "=" assignment ] ;
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
unary          = ( "-" | "!" | "~" ) unary | primary ;
primary        = INT_LIT | CHAR_LIT | STRING_LIT
               | IDENT | IDENT "(" [ args ] ")"
               | "(" expr ")" ;
args           = expr { "," expr } ;
```

Notas: `var_decl` dentro de `for_init` incluye su propio `;`. Los literales se definen en la sección 2. `char` es alias de `i8` en cualquier posición de `type`. Por leniencia el parser acepta una coma final en parámetros (`fn f(a : i64,)`) y argumentos (`f(a,)`), aunque la EBNF no la muestra. El cast es postfijo y asociativo por la izquierda (`x as i64 as u64` = `(x as i64) as u64`); el tipo tras `as` nunca puede ser `void`.

---

## Uso de la IR

La IR se vuelca con `--dump-ir` (`-I`). Solo se genera si lexer, parser y sema no reportan errores; con errores el compilador aborta antes (stdout vacío, exit distinto de cero). Los warnings no la bloquean: la IR se emite igual y el exit sigue siendo `0`.

Modelo:

- Un stream lineal por función (`IRFn.start`/`count`). Registros virtuales de 64 bits, numerados desde `0` en cada función (`reg_count` se reinicia por función; `label_count` es único en todo el módulo).
- No es SSA: un reg puede tener varias definiciones (variables, resultado de `&&` y `||`).
- Campos no usados = `NO_REG`. `imm64`/`target` comparten unión y nunca son registros.
- `data_type` = tipo de los OPERANDOS (tras las conversiones de §3: mismo tipo en aritmética/comparaciones salvo `&&`/`||`/`<<`/`>>`, que no unifican; en `<<`/`>>` es el tipo del operando izquierdo y el derecho conserva el suyo; decide signed/unsigned en `DIV`, `MOD`, `RSHIFT` y comparaciones). El resultado de comparaciones (`EQ..LE`), `NOT_L` (`lnot`), `&&` y `||` es siempre `i64` (0 o 1, canónico en cualquier tipo).
- Invariante de valores: todo reg con un tipo narrow (tamaño < 8) guarda la imagen canónica de 64 bits (extendido con signo si el tipo lo tiene, con ceros si no). Por eso `ADD`, `SUB`, `MUL`, `DIV`, `LSHIFT` y `NEG` sobre un narrow van seguidos de `extend` (trunca al ancho + extiende), mientras que `MOD`, `RSHIFT`, `AND`/`OR`/`XOR` y `NOT` sobre narrow con signo no lo llevan (el resultado ya queda canónico); `NOT` sobre narrow sin signo sí lo lleva. Los literales ya llegan con el tipo de destino (sema los reetiqueta) y se emiten como `const.<tipo>`.
- Cast (`NODE_CAST`, `expr as type`): solo emite código cuando el destino es narrow con origen de distinto tipo (un `extend.<tipo>` que trunca al ancho + extiende). Todo lo demás (hacia 64 bits, identidad) es no-op a nivel de IR porque todos los regs son de 64 bits y el narrow ya vive extendido: se reutiliza el reg del operando. Las conversiones implícitas de ensanche que inserta sema (init/asignación/arg/`return`) siguen la misma regla: `extend` si el destino es narrow, no-op si es de 64 bits. Como condición (`if`/`while`/`for`), el cast se evalúa a un reg y se salta con `JZ`/`JNZ` como cualquier otro valor.
- `gen_expr` nunca devuelve el reg de una variable: leer una global emite `LD_GLOBAL` a un temporal; leer una local copia con `MOVE` a un temporal. Declarar una local con init reutiliza el reg del resultado; sin init reserva un reg sin emitir nada.
- La asignación (`=`) evalúa primero el RHS, luego emite `MOVE` (local) o `STR_GLOBAL` (global), y devuelve el reg del RHS, por lo que `x = y = 5` comparte el mismo reg.
- Llamadas: los args se evalúan de izquierda a derecha a temporales, luego se emiten los `ARG` `0..argc-1` contiguos justo antes de su `CALL`. `CALL` a función `void` no tiene `dst` (se imprime sin `rN =`); con retorno, `dst` es un reg fresco.
- `&&` y `||` como valores se materializan a `0`/`1` (`i64`) con dos etiquetas (`l_false`/`l_end`); como condiciones (en `if`/`while`/`for`) compilan solo a saltos (`JZ`/`JNZ` + etiquetas auxiliares), sin materializar. `!` como condición invierte el sentido del salto.
- `if`/`elif`/`else`: cada condición salta a su `next` si es falsa; cada rama con más ramas detrás salta al `end` común. Un `if` sin `else` emite igualmente `next` y `end` (adyacentes si no hay `else`).
- `while (cond) body`: `L_cond:`, salta a `L_end` si `cond` es falsa, cuerpo, `jmp L_cond`, `L_end:`.
- `for (init; cond; updt) body`: `init`, `L_cond:`, si `cond` no es vacía salta a `L_end` si es falsa, cuerpo, `updt`, `jmp L_cond`, `L_end:`. Un slot vacío no emite nada (`for(;;)` no emite ningún salto de condición).
- Etiquetas con id único en todo el módulo. `JZ`/`JNZ` saltan si `src1 == 0` / `!= 0` (cualquier valor distinto de cero es verdadero).
- Toda función acaba en `RET` implícito `void` (aunque ya tenga `return` explícito). El código tras un `RET` se conserva (inalcanzable).
- Globals: se declaran en orden fuente pero sus inits se emiten en `init_order` (orden de resolución: una forward-ref fuerza el init referenciado antes). La función sintética `__r2_init` (siempre la primera, `: void`, `0 params`) contiene todos los inits (`gen_expr` + `STR_GLOBAL`) en ese orden. Sin globals, solo contiene `ret`.

Nombres en el volcado (`dump_ir`): `const`, `param`, `ld_global`, `str_global`, `move`, `extend`, `add`, `sub`, `mul`, `div`, `mod`, `and`, `or`, `xor`, `rshift`, `lshift`, `neg`, `not`, `lnot`, `eq`, `ne`, `gt`, `ge`, `lt`, `le`, `jmp`, `jz`, `jnz`, `arg`, `call`, `ret` (las etiquetas se imprimen como `Lx:`). El sufijo `.tipo` es el `data_type` (`ARG`, `JMP`/`JZ`/`JNZ` y `LABEL` no llevan sufijo; `CALL`/`RET` llevan el tipo de retorno). `char` nunca aparece como sufijo: el frontend lo convierte a `i8`. Formato:

```
global <nombre> : <tipo>
...
fn <nombre>(<n> params, <m> regs) : <ret>
    rN = const.<tipo> <imm>
    rN = param.<tipo> #indice
    rN = move.<tipo> rM
    rN = extend.<tipo> rM
    rN = ld_global.<tipo> @global
    str_global.<tipo> rM @global
    rN = <op>.<tipo> rA, rB
    rN = <op-un>.<tipo> rA
    arg rM #indice
    [rN = ]call[.<ret>] <fn>
    [ret[.<tipo>] [rM]]
    jmp -> Lx / jz|jnz rC -> Lx / Lx:
```

| op (dump) | dst | src1 | src2 | target/imm | type |
| --- | --- | --- | --- | --- | --- |
| `const` | r | - | - | imm64 | tipo del literal |
| `param` | r | - | - | target=índice | tipo del parámetro |
| `move` | r | r | - | - | tipo del valor |
| `extend` | r | r | - | - | narrow destino (trunca + extiende) |
| `ld_global` | r | - | - | target=global | tipo de la global |
| `str_global` | - | r | - | target=global | tipo de la global |
| `add` `sub` `mul` `div` `mod` `and` `or` `xor` `rshift` `lshift` | r | a | b | - | tipo de los operandos |
| `neg` `not` | r | a | - | - | tipo del operando |
| `lnot` | r | a | - | - | tipo del operando (resultado `i64`) |
| `eq` `ne` `gt` `ge` `lt` `le` | r | a | b | - | tipo de los operandos (resultado `i64`) |
| `label` | - | - | - | target=label | - |
| `jmp` | - | - | - | target=label | - |
| `jz` `jnz` | - | cond | - | target=label | - |
| `arg` | - | r | - | target=índice | - |
| `call` | r si ret `!= void`, si no `NO_REG` | - | - | target=fn, más `argc` | tipo de retorno |
| `ret` | - | r si hay valor, si no `NO_REG` | - | - | tipo del valor (`void` si no hay) |

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
| `-o` / `--out` | Ruta del assembly de salida |
| `-a` / `--arch` | Arquitectura del assembly generado|
| `-e` / `--execute` | Modo intérprete |

Nota: sin flags de dump, `r2p` compila y escribe el assembly a `out.s` (o a la ruta de `-o`; `-` es stdout). Hoy el backend x86-64 es un stub y el archivo sale vacío; `-a arm`, `-a riscv` y `-e` abortan con "backend is not implemented".

`make test` ejecuta las cuatro suites de fixtures (lexer, parser, sema, ir), cada una contra `build/r2p`.

Ejemplo de programa completo:

```r2
// factorial iterativo
fn fact(n : i64) : i64 {
  var r : i64 = 1;
  var i : i64 = 2;
  while (i <= n) {
    r = r * i;
    i = i + 1;
  }
  return r;
}

var start : i64 = 5;

fn main() : i64 {
  var f : i64 = fact(start);
  if (f == 120) {
    return 0;
  }
  return 1;
}
```

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
- Funciones solo a nivel top-level, con parámetros por valor.
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
while  return  as
```

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
- Se lexan y parsean, pero el type checker los rechaza: su semántica se define cuando existan los punteros.

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

- **Literales flexibles**: un literal (o `-literal`, o una expresión hecha solo de literales) adopta en silencio el tipo que le pide el contexto si su valor cabe en él: `var u : u64 = 1`, `take_i64('a')`, `c > 5` (el `5` se vuelve `i8`), `var v : u64 = 9223372036854775808` (sin sufijo, cabe en `u64`). Las subexpresiones puramente literales se **pliegan en compilación** con semántica de wrap, y el chequeo de rango se hace sobre el valor plegado: `var w : char = 100 * 100` es error (`literal 10000 does not fit in type i8`, reportado en el operador). Los casts de literales también pliegan (el valor se trunca al tipo del cast antes de plegar): `-(5 as u64)` es la constante `u64` exacta, `(300 as u8) > 200` pliega a `0`. El divisor cero conocido (literal, plegado o tras un cast) es **error de compilación** (`division by zero`, reportado en el divisor; `division by zero in constant expression`, reportado en el operador, si ambos operandos son literales). No se pliegan y se dejan al hardware: `div`/`mod` con signo por `-1` sobre el mínimo del tipo (`INT64_MIN / -1`, `(-128 as i8) / (-1 as i8)`) y los shifts con cuenta `>= 64`.
- **Ancho del plegado**: toda subexpresión hecha solo de literales (untyped) pliega con wrap a **64 bits**, sea cual sea el sabor del literal: `var x : i64 = 'a' + 'b';` da **195**. Solo los pares de operandos con tipo concreto narrow (p. ej. dos casts) pliegan con truncado al ancho de ese tipo (`(250 as u8) + 10` pliega a `4`, lo mismo que haría `add.u8` en runtime). Consecuencia: una constante que no cabe en el tipo destino es error aunque "envolvería" bien en un tipo más ancho (`var a : i8 = 1 << 7;` → `literal 128 does not fit in type i8`).
- **Locación de los errores de rango**: un literal suelto se reporta **en el literal** (`c + 300` marca el `300`); un `-literal` marca el `-literal` **completo**; una subexpresión plegada se reporta **en el operador** (el nodo plegado hereda la posición del operador), y un init en el nombre declarado.
- **Shifts con izquierda untyped**: si la cuenta es un literal, la constante plegada queda flexible y adopta el tipo del contexto (`var v : u64 = 1 << 2;` vale). Si la cuenta **no** es un literal, la izquierda finaliza a su tipo por defecto (`1 << x` es `i64`, `'a' << x` es `i8`) y el shift ya no adopta el contexto: `var a : i8 = 1 << x;` es un error de estrechamiento, igual que `var a : i8 = 1 << 7;`. Migración: `var v : u64 = 1 << x;` → `var v : u64 = (1 << x) as u64;` (la misma regla que ya aplicaba a `var v : u64 = 5 + x;`).
- **Literal fuera de rango**: error (`literal 300 does not fit in type i8; use an explicit cast with 'as' to reinterpret the bits`). Si además el destino es con signo y el valor supera `INT64_MAX`, el mensaje añade que el sufijo `u` lo convierte en literal `u64`. Un literal negativo hacia un destino sin signo es error, también dentro de binarios (`var u : u64 = -5;`, `5u + -1`); los mensajes siempre usan el nombre público del tipo (`u64`, nunca el interno). Un literal fuera de rango produce exactamente un diagnóstico, sin cascadas.
- **Literal `u` usado como tipo con signo**: solo un **warning** (`unsigned literal used as signed type i64; remove the 'u' suffix`); compila igual.
- **Ensanchar sin cambiar el valor es silencioso**: mismo signo a mayor tamaño (`i8`→`i64`, `u8`→`u32`), o sin signo a con signo mayor (`u8`→`i64`). Vale en binarios, init, asignación, args y `return`, sin cast explícito (el IR inserta un `extend` al cambiar de ancho).
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

De mayor a menor precedencia. Todos los binarios son asociativos por la izquierda excepto las asignaciones (`=`, `+=`, …), que lo son por la derecha.

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
| 1 | `=` `+=` `-=` `*=` `/=` `%=` `&=` `\|=` `^=` `<<=` `>>=` | tipo del LHS |

Los compuestos (`+=`, `-=`, `*=`, `/=`, `%=`, `&=`, `|=`, `^=`, `<<=`, `>>=`) son tokens propios, no `+` seguido de `=`; `<<=`/`>>=` son tokens de tres caracteres. `x OP= e` es exactamente `x = x OP e` (misma precedencia de asignación y misma asociatividad por la derecha).

El unario tiene la precedencia más alta y se aplica sobre un primario: `-x + y` es `(-x) + y`. `- -x` está permitido.
El cast está justo debajo del unario y por encima de todos los binarios (como en Rust: `unary > as > *`):
`-x as i64` es `(-x) as i64`, `1 as i64 + 2` es `(1 as i64) + 2`, `1 + 2 as u64` es `1 + (2 as u64)`.
Se encadena por la izquierda: `x as i64 as u64` es `(x as i64) as u64`. Los paréntesis fuerzan otro agrupamiento: `(1 + 2) as i64`.

---

## 5. Expresiones

- Literales enteros, char y string; identificadores; llamadas `f(a, b)`; expresiones entre paréntesis.
- **Cast `expr as type`** (cualquier tipo numérico, `char` = `i8`): conversión explícita que acepta cualquier operando no-`void` y devuelve exactamente el tipo escrito. Todas las combinaciones están permitidas (incluido el cast identidad `x as i64` con `x : i64`). Entre 64 bits (`i64<->u64`) es reinterpretación de bits (wrap/módulo 2^64); hacia un tipo narrow es truncado al ancho; hacia 64 bits desde un narrow es extensión con el signo del origen (`i8`→`i64` extiende con signo, `u8`→`u64` con ceros). El cast no es asignable: `(x as i64) = 1` es error (`left side of '=' must be a variable`). Como cualquier expresión, puede aparecer en inits, args, `return` y condiciones.
- **Asignación `=`**: es una expresión. El lado izquierdo debe ser una variable; el derecho debe ser del mismo tipo o ensanchar a él en silencio (ver punto 3; si no, error `cannot assign X to a variable of type Y; use an explicit cast with 'as'`, reportado en el `=`). Devuelve **el valor asignado**, por lo que puede encadenarse (`x = y = 1`) y usarse como expresión, incluso como condición.
- **Asignación compuesta `x OP= e`** (`+= -= *= /= %= &= |= ^= <<= >>=`): es una expresión que lee `x` una sola vez, calcula `x OP e` y guarda el resultado en `x`, devolviéndolo; puede encadenarse (`a += b += 1` es `a += (b += 1)`). El LHS debe ser una variable (si no, error `left side of compound assignment must be a variable`, reportado en el operador). Para `+= -= *= /= %= &= |= ^=`, el RHS se adapta al tipo del LHS con las reglas de asignación (mismo tipo, literal flexible o ensanche implícito); si no, error `cannot assign X to a variable of type Y; use an explicit cast with 'as'` en el operador, igual que `x = x OP e`. `<<=` y `>>=` siguen las reglas del desplazamiento binario: el RHS puede ser cualquier numérico y el resultado tiene el tipo del LHS. `x /= 0` y `x %= 0` reportan `division by zero` en el `0`.
- **`&&` y `||`**: short-circuit. Si el resultado queda decidido por el operando izquierdo, el derecho no se evalúa. Un operando constante emite un **warning** (`constant operand of '&&' is always true/false`), también a través de un cast (`(1 as i64) && x` advierte).

---

## 6. Statements

- Declaración de variable: `var ID : type (= expr)? ;`
- Expresión: `expr ;`
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
  - `while`/`for` garantizan solo si su condición es constante verdadera (`while (1)`, `for (;;)`): sin `break`, el bucle solo sale con `return`. Con condición no constante nunca garantizan.

---

## 9. Globals

- `var ID : type (= expr)? ;` a nivel top-level. Cualquier otra cosa a nivel top-level es error: `only variable and function declarations are allowed at the top level`.
- Todas se declaran antes de chequear los inits: las **forward references están permitidas**.
- Referencias a variables globales futuras permitido, ej: `var x:i64 = y; var y:i64 = 73` es válido
- Una global **sin init vale 0** en runtime (vive en `.bss`), pero su lectura **sí advierte** igual que una local sin asignar: solo la evitan un init o una asignación previa en el flujo (una asignación en una función chequeada antes también la silencia, en orden fuente).
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

El codegen aplica además optimizaciones locales (operaciones read-modify-write, fusión de comparación con salto, tail calls) en todos los niveles; `-O0` fuerza que los valores vivan en pila.

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
               | inc_dec_stmt
               | expr ";" ;
inc_dec_stmt   = IDENT ( "++" | "--" ) ";" ;
var_decl       = "var" IDENT ":" type [ "=" expr ] ";" ;
if_stmt        = "if" "(" expr ")" statement
                 { "elif" "(" expr ")" statement }
                 [ "else" statement ] ;
while_stmt     = "while" "(" expr ")" statement ;
for_stmt       = "for" "(" for_init for_cond for_updt ")" statement ;
for_init       = var_decl | [ expr ] ";" ;
for_cond       = [ expr ] ";" ;
for_updt       = [ expr | IDENT ( "++" | "--" ) ] ;
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
unary          = ( "-" | "!" | "~" ) unary | primary ;
primary        = INT_LIT | CHAR_LIT | STRING_LIT
               | IDENT | IDENT "(" [ args ] ")"
               | "(" expr ")" ;
args           = expr { "," expr } ;
```

Notas: `var_decl` dentro de `for_init` incluye su propio `;`. Los literales se definen en la sección 2. `char` es alias de `i8` en cualquier posición de `type`. Por leniencia el parser acepta una coma final en parámetros (`fn f(a : i64,)`) y argumentos (`f(a,)`), aunque la EBNF no la muestra. El cast es postfijo y asociativo por la izquierda (`x as i64 as u64` = `(x as i64) as u64`); el tipo tras `as` nunca puede ser `void`.

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

Nota: sin flags de dump, `r2p` compila y escribe el assembly a `<fuente>.s` (o a la ruta de `-o`; `-` es stdout). `-a arm`, `-a riscv` y `-e` abortan en codegen con `Error: <backend> backend is not implemented` (`ARM`, `RISC-V` o `interpreter`). Los modos de dump no pasan por codegen, así que pueden inspeccionar un programa aunque la arquitectura elegida no tenga backend. Si se pasan varios ficheros, solo se compila el primero y se avisa por stderr.

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

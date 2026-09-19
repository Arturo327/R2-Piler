# R2-Lang — Especificación del lenguaje

Especificación completa de R2-Lang, el lenguaje que compila R2-Piler.

Estado: lexer, parser y sema implementados y testeados. IR, `optimize_ir` y codegen pendientes.

---

## 1. Panorama

R2-Lang es un lenguaje imperativo, estática y fuertemente tipado, sin conversiones implícitas.

- Tipos primitivos: `i64`, `u64`, `char` (1 byte con signo), `void`.
- Funciones solo a nivel top-level, con parámetros por valor.
- Scopes de bloque con shadowing entre scopes.
- Entry point: `fn main() : i64`.
- El compilador emite assembly x86-64, no binarios: el binario final se produce ensamblando.

Pipeline del compilador:

```
lexer -> parser -> sema -> IR -> optimize_ir -> codegen (assembly)
```

Un error en cualquier fase aborta la compilación antes de la siguiente. Los warnings no abortan.

---

## 2. Estructura léxica

### 2.1 Comentarios

`//` hasta el fin de línea. No existen comentarios de bloque.

### 2.2 Identificadores y keywords

Identificadores: `[a-zA-Z_][a-zA-Z0-9_]*`.

Keywords (una palabra clave nunca puede usarse como identificador):

```
fn  if    for    i64  u64    var
char  elif  else  void  while  return
```

### 2.3 Literales enteros

- Decimal: `42`. Hexadecimal: `0x10` / `0X10`. Octal: `0o17` / `0O17`. Binario: `0b101` / `0B101`.
- Los decimales con ceros a la izquierda (`007`) son decimales, no octales. `0` solo es decimal.
- Sufijo `u` / `U` → `u64`; sin sufijo → `i64`.
- Un literal sin sufijo mayor que `INT64_MAX` es error (se sugiere añadir `u`).
- Overflow en cualquier base o dígito inválido para la base: error.

### 2.4 Literales char

- `'c'`, exactamente un carácter. Vacío, multichar, newline dentro o sin cerrar: error.
- Escapes válidos: `\n` `\t` `\r` `\0` `\\` `\'` `\"`. Escapes desconocidos: error.
- `char` es 1 byte **con signo**: rango -128..127. Al operar o comparar a 64 bits se hace sign-extend.

### 2.5 Literales string

- `"..."` con los mismos escapes que char, más `\<newline>` como continuación de línea.
- Un string no puede contener un newline real sin escapar.
- **Hoy:** se lexan y parsean, pero el type checker los rechaza. Su semántica se define cuando exista punteros.

---

## 3. Tipos

| Tipo | Tamaño | Rango / representación |
|------|--------|------------------------|
| `i64` | 8 bytes | Entero con signo, complemento a 2. Aritmética con wrap definido. |
| `u64` | 8 bytes | Entero sin signo, módulo 2^64. |
| `char` | 1 byte | Con signo, -128..127. Sign-extend al operar a 64 bits. |
| `void` | — | Solo como tipo de retorno de función. |

Reglas de tipos (estrictas, sin coerciones):

- Operadores aritméticos y bitwise (`+ - * / % & | ^ << >>`): ambos operandos del **mismo tipo**, resultado del tipo de los operandos.
- Comparaciones (`< > <= >= == !=`) y lógicos (`&& || !`): resultado siempre `i64` (0 o 1).
- `-` y `~` unarios: preservan el tipo del operando. `!` unario: resultado `i64`.
- `void` no puede aparecer en expresiones (ni en condiciones, ni como operando, ni como init).

---

## 4. Operadores: precedencia y asociatividad

De mayor a menor precedencia. Todos los binarios son asociativos por la izquierda excepto `=` (por la derecha).

| Prec | Operadores | Resultado |
|------|-----------|-----------|
| unario | `-` `!` `~` (prefijo) | `-`/`~` preservan tipo, `!` → `i64` |
| 11 | `*` `/` `%` | tipo de los operandos |
| 10 | `+` `-` | tipo de los operandos |
| 9 | `>>` `<<` | tipo de los operandos |
| 8 | `<` `>` `<=` `>=` | `i64` |
| 7 | `==` `!=` | `i64` |
| 6 | `&` | tipo de los operandos |
| 5 | `^` | tipo de los operandos |
| 4 | `\|` | tipo de los operandos |
| 3 | `&&` | `i64` |
| 2 | `\|\|` | `i64` |
| 1 | `=` | tipo del LHS |

El unario tiene la precedencia más alta y se aplica sobre un primario: `-x + y` es `(-x) + y`. `- -x` está permitido.

---

## 5. Expresiones

- Literales enteros, char y string; identificadores; llamadas `f(a, b)`; expresiones entre paréntesis.
- **Asignación `=`**: es una expresión. El lado izquierdo debe ser una variable; el derecho debe tener exactamente el mismo tipo. Devuelve **el valor asignado**, por lo que puede encadenarse (`x = y = 1`) y usarse como expresión, incluso como condición.
- **`&&` y `||`**: short-circuit. Si el resultado queda decidido por el operando izquierdo, el derecho no se evalúa. Compilan a una serie de saltos, no a operaciones aritméticas.

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
- `return expr? ;` — en una función `void`: `return;`. En una función `T`: `return expr;` con tipo exacto.

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
- Las llamadas comprueban arity y tipos exactos, parámetro a parámetro.
- **Return obligatorio en todos los caminos** para funciones no-`void` (chequeo conservador):
  - Un `return` garantiza.
  - Un bloque garantiza si alguno de sus statements garantiza.
  - Un `if` garantiza solo si tiene `else` final y todas las ramas (then/elif/else) garantizan.
  - `while`/`for` nunca garantizan: un `while (1) { return x; }` sigue reportando "may reach the end without returning".

---

## 9. Globals

- `var ID : type (= expr)? ;` a nivel top-level. Otra cosa a nivel top-level es error.
- Todas se declaran antes de chequear los inits: las **forward references están permitidas**.
- Referencias a variables globales futuras permitido, ej: `var x:i64 = y; var y:i64 = 73` es válido
- **Auto-referencia** (`var x : i64 = x;`): error.
- **Cadenas circulares** entre inits (`var y : i64 = x; var x : i64 = y;`): error. Al chequear el init de una global, si el init referencia a otra global cuyo init aún no se chequeó, ese init se chequea en ese momento (recursivamente); referenciar una global cuyo init está a medio chequear es el ciclo, y se reporta en el ref que lo cierra (un error por ciclo).
- Solo se siguen **referencias directas** a globals en los inits. Los ciclos a través de llamadas a funciones dentro de inits no se detectan.
- Los inits pueden llamar funciones; esas llamadas se ejecutan en orden fuente durante el arranque.

---

## 10. Condiciones

- Toda condición (`if`, `elif`, `while`, `for`) es una expresión de tipo no-`void`.
- Cualquier valor **distinto de cero es verdadero**; cero es falso.
- La cond vacía de `for` cuenta como verdadera.

---

## 11. Entry point y salida

- El entry point es exactamente `fn main() : i64`, sin parámetros. Su valor de retorno es el exit code del programa.
- Si el programa no define `main`: se genera assembly igualmente.
- La salida del compilador es **assembly x86-64**, no un binario. El binario final se produce ensamblando (p. ej. `gcc out.s`).

---

## 12. Comportamiento indefinido

No se añade ninguna lógica para evitarlo; el resultado es lo que haga el hardware:

- Shifts con cuenta `>= 64`.
- División o módulo por cero: fault del hardware (`SIGFPE`).
- Leer una local sin asignar: valor indefinido.

El **wrap** aritmético NO es indefinido: está definido como complemento a 2 (y módulo 2^64 para `u64`).

---

## 13. IR y optimizaciones

- Toda la AST válida se baja a IR, incluido el código inalcanzable tras un `return`.
- `optimize_ir` es una pasada posterior que hace poda de código muerto, eliminación de código inalcanzable y optimizaciones.

---

## 14. Gramática (EBNF)

```
program        = { fn_decl | var_decl } ;
fn_decl        = "fn" IDENT "(" [ params ] ")" [ ":" ret_type ] block ;
params         = param { "," param } ;
param          = IDENT ":" type ;
ret_type       = "void" | type ;
type           = "i64" | "u64" | "char" ;
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
multiplicative = unary { ( "*" | "/" | "%" ) unary } ;
unary          = ( "-" | "!" | "~" ) unary | primary ;
primary        = INT_LIT | CHAR_LIT | STRING_LIT
               | IDENT | IDENT "(" [ args ] ")"
               | "(" expr ")" ;
args           = expr { "," expr } ;
```

Notas: `var_decl` dentro de `for_init` incluye su propio `;`. Los literales se definen en la sección 2.

---

## Uso de la IR

- Un stream lineal por función (`IRFn.start`/`count`). Registros virtuales de 64 bits.
- No es SSA: un reg puede tener varias definiciones (variables, resultado de `&&` y `||`).
- Campos no usados = `NO_REG`. `imm64`/`target` comparten unión y nunca son registros.
- `data_type` = tipo de los OPERANDOS (decide signed/unsigned en `DIV`, `MOD`, `RS` y comparaciones). Resultado de comparaciones, `NOT_L`, `&&` y `||` es siempre `i64`.
- Un `char` vive siempre en su reg extendido con signo (-128..127): `ADD`, `SUB`, `MUL`, `DIV`, `LS`, `NEG` sobre `char` van seguidos de `SEXT8`.
- `gen_expr` nunca devuelve el reg de una variable: leer una local copia con `MOVE`.
- Los `ARG` 0..`argc`-1 van contiguos justo antes de su `CALL`.
- Etiquetas con id único en todo el módulo. `JZ`/`JNZ` saltan si `src1 == 0` / `!= 0`.
- Toda función acaba en `RET`. El código tras un `RET` se conserva (inalcanzable).

| op | dst | src1 | src2 | target/imm | type |
| --- | --- | --- | --- | --- | --- |
| `CONST` | r | - | - | imm64 | tipo |
| `PARAM` | r | - | - | target=indice | tipo |
| `MOVE` | r | r | - | - | tipo |
| `SEXT8` | r | r | - | - | char |
| `LD_GLOBAL` | r | - | - | target=global | tipo |
| `ST_GLOBAL` | - | r | - | target=global | tipo |
| `ADD..LS` | r | a | b | - | tipo operandos |
| `NEG NOT_A` | r | a | - | - | tipo operando |
| `NOT_L` | r | a | - | - | tipo operando (res i64) |
| `EQ..LE` | r | a | b | - | tipo operandos (res i64) |
| `LABEL` | - | - | - | target=label | - |
| `JMP` | - | - | - | target=label | - |
| `JZ JNZ` | - | cond | - | target=label | - |
| `ARG` | - | r | - | target=indice | - |
| `CALL` | r|NO | - | - | target=fn, argc | ret |
| `RET` | - | r|NO | - | - | ret |

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
| `-o` / `--out` | Ruta del assembly de salida |

`make test` ejecuta las tres suites de fixtures (lexer, parser, sema), cada una contra `build/r2p`.

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

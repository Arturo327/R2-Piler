// P4: peepholes de forma. `xorl` materializa los ceros iniciales e
// `incq`/`decq` sustituyen add/sub $1 (mismo número de instrucciones, menos
// bytes), incluido el RMW en sitio del bucle. El edge condicional del bucle
// no necesita stub `.LE` (no hay moves).
fn main() : i64 {
	var a : i64 = 0;
	var b : i64 = 10;
	var i : i64 = 0;

	while (i < 10) {
		a = a + 1;
		b = b - 1;
		i = i + 1;
	}

	if (a != 10) { return 1; }
	if (b != 0) { return 2; }
	return a + b - 10;
}
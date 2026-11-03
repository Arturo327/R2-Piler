// P3: parámetros estrechos (sin `normalize_params`) usados a su ancho y
// ensanchados explícitamente a i64. `a` es i8 con signo (debe sign-extenderse
// al ensanchar) y `b` es u8 (zero-extend).
fn mix(a : i8, b : u8, n : i64) : i64 {
	var s : i64 = 0;
	var i : i64 = 0;

	while (i < n) {
		s = s + (a as i64) + (b as i64);
		a = a + 1;
		b = b + 2;
		i = i + 1;
	}
	return s;
}

fn main() : i64 {
	if (mix(0 - 3, 250, 4) != 750) { return 1; }
	if (mix(0 - 128, 0, 1) != (0 - 128)) { return 2; }
	return 0;
}
// P2: límite del área entrante. `wrap8` tiene 7 parámetros (avail = 1) pero
// llama a `add8` con 8 argumentos (extra = 2), así que no cabe y la llamada
// debe seguir siendo un `call` real con sus 2 argumentos de pila.
fn add8(a : i64, b : i64, c : i64, d : i64, e : i64, f : i64, g : i64,
		h : i64) : i64 {
	return a + b + c + d + e + f + g + h;
}

fn wrap8(n : i64, a : i64, b : i64, c : i64, d : i64, e : i64, f : i64) : i64 {
	if (n <= 0) { return 0; }
	return add8(1, a, b, c, d, e, f, n);
}

fn main() : i64 {
	if (wrap8(3, 10, 20, 30, 40, 50, 60) != 214) { return 1; }
	if (wrap8(0, 10, 20, 30, 40, 50, 60) != 0) { return 2; }
	return 0;
}
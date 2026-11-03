// P2: wrapper de 7 parámetros que tail-llama a otra función de 7 parámetros.
// El 7º argumento (`g + n`) se escribe en el área entrante antes del
// `leave; jmp` (avail = 1, extra = 1).
fn add7(a : i64, b : i64, c : i64, d : i64, e : i64, f : i64, g : i64) : i64 {
	return a + b + c + d + e + f + g;
}

fn wrap7(n : i64, a : i64, b : i64, c : i64, d : i64, e : i64, g : i64) : i64 {
	if (n <= 0) { return g; }
	return add7(n, a, b, c, d, e, g + n);
}

fn main() : i64 {
	if (wrap7(5, 1, 2, 3, 4, 5, 0) != 25) { return 1; }
	if (wrap7(1, 10, 20, 30, 40, 50, 60) != 212) { return 2; }
	return 0;
}
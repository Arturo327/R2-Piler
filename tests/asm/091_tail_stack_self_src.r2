// P2: tail call con 7 argumentos (el séptimo va en pila). `sum7` tiene 7
// parámetros, así que dispone de un slot entrante (avail = 1) y el self-tail
// se compila a `jmp .LS` escribiendo `acc` en su propio slot de pila: sin
// `call` y sin crecer la pila.
fn sum7(n : i64, a : i64, b : i64, c : i64, d : i64, e : i64, acc : i64) : i64 {
	if (n <= 0) {
		return acc + a + b + c + d + e;
	}
	return sum7(n - 1, a, b, c, d, e, acc + n);
}

fn main() : i64 {
	// sum(1..1000) = 500500, más a+b+c+d+e = 15.
	if (sum7(1000, 1, 2, 3, 4, 5, 0) != 500515) { return 1; }
	return 0;
}
// P5: función no hoja (llamada no-tail => frame > 0) con 7 parámetros, el
// último en pila. Sin %rbp: slots y argumento entrante se direccionan desde
// %rsp (frame + 8). add2 impide que el frame se quede en red zone.
fn add2(a : i64, b : i64) : i64 {
	return a + b;
}

fn fwd7(a : i64, b : i64, c : i64, d : i64, e : i64, f : i64, g : i64) : i64 {
	var s : i64 = add2(a, b);

	return s + c + d + e + f + g;
}

fn main() : i64 {
	if (fwd7(1, 2, 3, 4, 5, 6, 7) != 28) { return 1; }
	return 0;
}
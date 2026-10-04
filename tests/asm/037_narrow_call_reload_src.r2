// REGRESION B2: recarga de un operando angosto tras call.
// `t = a` (i8) debe recargarse tras el segundo `get()`: sin invalidar
// `rax_v` en stores angostos/calls, el MOVE reutilizaba basura y daba 3.
var n : i8 = 0;
fn get() : i8 {
	n = n + 1;
	return n;
}
fn main() : i64 {
	var a : i8 = get();
	var t : i8 = a;
	var b : i8 = get();
	if (a != 1) {
		return 1;
	}
	if (b != 2) {
		return 2;
	}
	if (t != 1) {
		return 3;
	}
	return 0;
}

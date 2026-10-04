// REGRESION B2: recarga de un operando angosto tras LD_GLOBAL.
// `t = x` (i8, param) debe sobrevivir al `LD_GLOBAL g`: sin invalidar,
// el MOVE posterior saltaba su `movsbq` y `f(3)` devolvia basura en vez de 3.
var g : i8 = 5;
fn f(x : i8) : i64 {
	var t : i8 = x;
	var q : i8 = g;
	if (x != 3) {
		return 1;
	}
	if (q != 5) {
		return 2;
	}
	return t;
}
fn main() : i64 {
	return f(3);
}

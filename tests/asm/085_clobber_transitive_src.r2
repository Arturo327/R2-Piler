// P1.a: el resumen de clobbers es transitivo (como gtouch). main llama a
// a -> b -> c; c usa seis locals, así que ensucia registros caller-saved que
// main no puede asumir intactos a través de la llamada. k/m viven en el
// bucle y se comprueban al final.
fn c(x : i64) : i64 {
	var p : i64 = x + 1;
	var q : i64 = x + 2;
	var r : i64 = x + 3;
	var u : i64 = x + 4;
	var v : i64 = x + 5;
	var w : i64 = x + 6;

	return p + q + r + u + v + w;
}

fn b(x : i64) : i64 {
	return c(x) + 1;
}

fn a(x : i64) : i64 {
	return b(x) + 1;
}

fn main() : i64 {
	var s : i64 = 0;
	var i : i64 = 0;
	var k : i64 = 7;
	var m : i64 = 9;

	while (i < 40) {
		s = s + a(i) + k + m;
		k = k + 1;
		m = m + 2;
		i = i + 1;
	}

	if (s != 8580) { return 1; }
	if (k != 47 || m != 89) { return 2; }
	return 0;
}
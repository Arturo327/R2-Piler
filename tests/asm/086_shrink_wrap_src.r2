// P1.b: shrink-wrapping de callee-saved. El camino `n <= 0` sale antes de la
// región que usa los registros callee-saved, así que sus saves se mueven
// detrás del guard (con el edge move de `n` como única excepción). Ambos
// caminos se ejecutan y se comprueban.
fn step(x : i64) : i64 {
	return (x * 3 + 1) & 65535;
}

fn work(n : i64) : i64 {
	if (n <= 0) { return 7; }

	var a : i64 = 1;
	var b : i64 = 2;
	var c : i64 = 3;
	var d : i64 = 4;
	var e : i64 = 5;
	var f : i64 = 6;
	var s : i64 = 0;
	var i : i64 = 0;

	while (i < n) {
		s = s + a + b + c + d + e + f + step(i);
		a = (a + b) & 65535;
		b = (b + c) & 65535;
		c = (c + d) & 65535;
		d = (d + e) & 65535;
		e = (e + f) & 65535;
		f = (f + i) & 65535;
		i = i + 1;
	}
	return s & 255;
}

fn main() : i64 {
	if (work(0) != 7) { return 1; }
	if (work(5) != 110) { return 2; }
	return 0;
}
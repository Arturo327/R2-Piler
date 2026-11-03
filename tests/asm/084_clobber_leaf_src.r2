// P1.a: clobbers exactos. `step` es una hoja que solo escribe rax/r11; los
// valores vivos a través de la llamada que están en homes caller-saved no
// argumento no necesitan save/restore. a,b,c,d se usan después de cada
// llamada, así que un clobber mal calculado cambia el resultado.
fn step(x : i64) : i64 {
	return x * 3 + 1;
}

fn main() : i64 {
	var a : i64 = 1;
	var b : i64 = 2;
	var c : i64 = 3;
	var d : i64 = 4;
	var s : i64 = 0;
	var i : i64 = 0;

	while (i < 50) {
		s = s + a + b + c + d;
		s = s + step(i);
		a = a + 1;
		b = b + 2;
		c = c + 3;
		d = d + 4;
		i = i + 1;
	}

	if (s != 16475) { return 1; }
	if (a != 51 || b != 102 || c != 153 || d != 204) { return 2; }
	return 0;
}
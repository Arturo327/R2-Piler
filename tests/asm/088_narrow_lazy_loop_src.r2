// P3: representación raw de tipos estrechos. Las operaciones de ancho nativo
// (add/sub/and/cmp) no re-extienden la imagen en cada iteración; la extensión
// a 64 bits ocurre solo en el consumidor (los `as i64` de la suma).
fn main() : i64 {
	var a : i8 = 1;
	var b : i16 = 300;
	var c : i32 = 70000;
	var s : i64 = 0;
	var i : i64 = 0;

	while (i < 100) {
		a = a + 3;
		b = b - 7;
		c = c + 1000;
		s = s + (a as i64) + (b as i64) + (c as i64);
		i = i + 1;
	}

	if (a != 45) { return 1; }
	if (b != (0 - 400)) { return 2; }
	if (c != 170000) { return 3; }
	if (s != 12045052) { return 4; }
	return 0;
}
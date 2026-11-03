// P3: consumidores de 64 bits sobre valores estrechos raw: shift con contador
// variable (se materializa el valor extendido), división por constante con
// signo (magia a 64 bits) y shift/ mod sin signo de u16.
fn main() : i64 {
	var x : i32 = 0 - 12345;
	var sh : i32 = 3;
	var y : i32 = x >> sh;
	var z : i32 = x / 7;
	var w : u16 = 40000;
	var v : u16 = w >> 2;
	var u : u16 = w % 700;

	if (y != (0 - 1544)) { return 1; }
	if (z != (0 - 1763)) { return 2; }
	if (v != 10000) { return 3; }
	if (u != 100) { return 4; }
	return 0;
}
// presion de registros con div/mod/shift variables y una llamada dentro del bucle
fn mix (a : i64, b : i64) : i64 { return (a * 31 + b) & 1048575; }

fn main () : i64
{
	var a : i64 = 7;
	var b : i64 = 11;
	var c : i64 = 13;
	var d : i64 = 17;
	var e : i64 = 19;
	var f : i64 = 23;
	var g : i64 = 29;
	var s : i64 = 0;

	for (var i : i64 = 1; i <= 50; i = i + 1) {
		s = s + (a / ((b & 7) + 1)) + (c % ((d & 15) + 1));
		a = (a + (e << (f & 7)) + i) & 65535;
		b = (b ^ (s >> 3)) & 65535;
		c = (c + mix(a, g) + (i % 5)) & 65535;
		d = (d + (s % 11) + (a >> (b & 3))) & 65535;
		e = (e + (d / 3) + i) & 65535;
		f = (f + b + c) & 65535;
		g = (g ^ (e + f)) & 65535;
		s = s & 16777215;
	}
	return ((s ^ (s >> 8) ^ (s >> 16)) + a + (b >> 3) + c + (d >> 5) + e + f + g) & 255;
}

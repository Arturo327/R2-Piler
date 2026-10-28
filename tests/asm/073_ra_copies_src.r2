// cadenas de copias, argumentos, retornos y if/else sobre variables (coalescing y hints)
fn add3 (a : i64, b : i64, c : i64) : i64 { return a + b * 2 + c * 3; }

fn pick (c : i64, a : i64, b : i64) : i64
{
	var r : i64;
	if (c) { r = a; } else { r = b; }
	return r;
}

fn main () : i64
{
	var x : i64 = 5;
	var y : i64 = x;
	var z : i64 = y;
	var s : i64 = 0;

	for (var i : i64 = 0; i < 20; i = i + 1) {
		var t : i64 = x;
		x = y + i;
		y = z + t;
		z = t ^ x;
		s = s + add3(x & 255, y & 255, z & 255);
		if (s & 1) { t = x; } else { t = y; }
		s = s + t + pick(i & 1, x & 15, y & 15);
	}
	return s & 255;
}

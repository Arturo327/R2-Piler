fn main () : i64 {
	var x : i64 = 10;
	var y : i64 = 20;
	var p : ptr i64 = $x;
	var pp : ptr ptr i64 = $p;
	p[0] = 1;
	p[1] += 2;
	p[2]++;
	var a : i64 = p[1];
	var b : i64 = pp[0][1];
	var c : ptr i64 = $p[2];
	var d : i64 = sizeof(p[3]);
	return a + b + c[0] + d + y;
}

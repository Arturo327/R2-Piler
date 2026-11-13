struct P { x : i64; y : i64; }

fn bump(p : ptr i64) : void {
	@p = 3;
}

fn main() : i64 {
	var p : P = {.x = 1, .y = 2};
	p.x = p.x + 1;
	var a : i64[4] = {1, 2, 3, 4};
	a[1] = a[0] + 1;
	var i : i64 = 2;
	var d : i64 = a[i];
	a[i] = 9;
	var q : i64[2];
	bump(q);
	q[0] = 1;
	var r : i64 = q[0];
	var e : i64[4];
	var addr : ptr i64 = $e[1];
	e[0] = 1;
	var ev : i64 = addr[0];
	var w : i64[2];
	if (i > 0) { w[0] = 1; }
	var wv : i64 = w[0];
	return p.x + a[0] + d + r + ev + wv;
}

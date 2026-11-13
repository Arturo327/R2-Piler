struct P { x : i64; y : i64; }

fn main() : i64 {
	var p : P;
	var s : i64 = p.x;
	var a : i64[4];
	var u : i64 = a[2];
	var q : P = {.x = 1, .y = 2};
	q.y = 7;
	q.y = 8;
	var b : i64[4] = {1, 2, 3, 4};
	b[0] = 5;
	b[0] = 6;
	var unused_s : P;
	var unused_a : i64[2];
	return s + u + q.y + b[0];
}

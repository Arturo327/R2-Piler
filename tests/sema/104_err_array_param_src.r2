fn take(x : i32[3]) : i64 {
	return x[0] as i64;
}

fn bad(v : i32[], p : ptr i32, a : i32[2], b : i64[3], c : i32[3]) : i64 {
	var t : i64 = take(v);
	var u : i64 = take(p);
	var w : i64 = take(a);
	var y : i64 = take(b);
	var z : i64 = take(c);
	var m : i64 = c * 2;
	var n : i64 = c == c;
	var o : i64 = c + c;

	return t + u + w + y + z + m + n + o;
}

fn main() : i64 {
	arr g : i32[3] = {1, 2, 3};

	return take(g);
}

fn foo (a : i64, b : i64 = 4, c : i64 = 6) : i64 {
	return a * 100 + b * 10 + c;
}

fn main () : i64 {
	var x : i64 = 1;
	var r : i64 = 0;
	r += foo(x, 2, c = 4);
	r += foo(x, b = 2, 3);
	r += foo(x, c = 3);
	r += foo(x, 2);
	r += foo(a = x, b = 2);
	return r;
}

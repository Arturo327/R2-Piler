fn f() : i64 {
	return 1;
}

var g : i64 = 3;

static var top : i64 = 1;

fn main() : i64 {
	var l : i64 = 2;

	static var a : i64 = f();
	static var b : i64 = g;
	static var c : i64 = l;
	static var ok : i64 = 1 + 2;
	static var d : i64 = ok;
	static var q : ptr i64 = $l;
	static arr sl : i64[] = {1};
	static var e : i64 = 5;
	static var e : i64 = 6;
	return 0;
}
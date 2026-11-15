// Go-style variadics: the extras become a slice inside the callee. Covers no
// extras, one extra, several extras, struct and fixed-array elements, a fixed
// parameter before the variadic one, and more than six arguments (the array
// itself is one pointer, so the callee still sees two slots).
struct P { x : i64; y : i64; }

fn sum (rest : ...i64) : i64 {
	var s : i64 = 0;
	for (var i : rest) { s += rest[i]; }
	return s;
}

fn total (ps : ...P) : i64 {
	var s : i64 = 0;
	for (var i : ps) { s += ps[i].x * ps[i].y; }
	return s;
}

fn pairs (vs : ...i64[2]) : i64 {
	var s : i64 = 0;
	for (var i : vs) { s += vs[i][0] + vs[i][1]; }
	return s;
}

fn count (tag : i64, rest : ...i32) : i64 { return tag * 100 + rest.len; }

pub fn main () : i64 {
	var a : i64[2] = {3, 4};
	var b : i64[2] = {5, 6};
	var r : i64 = 0;
	r += sum();
	r += sum(5);
	r += sum(1, 2, 3, 4);
	r += total({.x = 1, .y = 2} as P, {.x = 3, .y = 4} as P);
	r += pairs(a, b);
	r += count(1, 1, 2, 3);
	r += sum(1, 2, 3, 4, 5, 6, 7, 8, 9, 10);
	return r;
}
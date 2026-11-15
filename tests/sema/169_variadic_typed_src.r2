// '...T' becomes a T[] parameter: rest.len, rest[i], the index form of 'for'
// and forwarding rest to a slice parameter all work, and a call accepts zero
// or more arguments of the element type (structs and fixed arrays included).
struct P { x : i64; y : i64; }

fn sum (rest : ...i64) : i64 {
	var total : i64 = 0;
	for (var i : rest) { total += rest[i]; }
	return total;
}

fn forward (rest : ...i64) : i64 { return sum(rest[0], rest[1]); }
fn total (ps : ...P) : i64 { return ps.len as i64; }
fn pairs (vs : ...i64[2]) : i64 { return vs.len as i64; }
fn show (_tag : ptr i8, rest : ...i32) : i64 { return rest.len as i64; }

pub fn main () : i64 {
	var a : i64[2] = {1, 2};
	sum();
	sum(1, 2, 3);
	forward(4, 5);
	total({.x = 1, .y = 2} as P);
	pairs(a);
	show("x", 1, 2, 3);
	return 0;
}
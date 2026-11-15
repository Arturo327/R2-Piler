// A typed variadic call packs the extra arguments into a stack array of type
// T[n] and passes it as a slice (pointer + length); with no extras it passes
// a null pointer and length 0.
fn sum (rest : ...i64) : i64 { return rest.len; }
fn show (tag : i64, _rest : ...i32) : i64 { return tag; }

pub fn main () : i64 {
	var n : i64 = sum(1, 2, 3);
	n += show(7, 8);
	n += show(9);
	return n;
}
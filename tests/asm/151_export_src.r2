export fn add (a : i64, b : i64) : i64 { return a + b; }
export var base : i64 = 10;

pub fn main () : i64 {
	return add(base, 5) - 10;
}

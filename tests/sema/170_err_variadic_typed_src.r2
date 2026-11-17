// Typed variadics: extern/export functions cannot use them, the element type
// cannot be a slice or a reference, defaults are not allowed, and a call
// checks every extra against the element type.
extern fn bad (x : ...i64) : i64;

export fn expo (_x : ...i64) : i64 { return 0; }

fn slice_elem (_x : ...i64[]) : i64 { return 0; }
fn ref_elem (_x : ...&i64) : i64 { return 0; }
fn defs (a : i64 = 1, _rest : ...i64) : i64 { return a; }

fn need (a : i64, _rest : ...i64) : i64 { return a; }

fn sum (rest : ...i64) : i64 {
	var s : i64 = 0;
	for (var i : rest) { s += rest[i]; }
	return s;
}

pub fn main () : i64 {
	arr av : i64[3] = {1, 2, 3};
	sum("x");
	sum(1, "x");
	sum(rest = 1);
	need();
	sum(av);
	bad(1, 2);
	return 0;
}
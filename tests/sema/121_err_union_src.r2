struct Tagged {
	tag: i8;
	union {
		whole: u32;
		struct {
			lo: u16;
			hi: u16;
		}
	}
}

union Pair {
	a: i32;
	b: i64;
}

fn main() : i64 {
	var a : Tagged = {.tag = 1, .whole = 2, .lo = 3};
	var b : Tagged = {.tag = 1};
	var c : Pair = {.a = 1};
	return (a.tag + b.tag) + (c.a as i64);
}

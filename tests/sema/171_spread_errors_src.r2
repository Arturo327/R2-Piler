arr ga : i64[2] = {1, 2};

fn plain (a : i64) : i64 { return a; }
extern fn printf (fmt : ptr i8, ...) : i32;
fn typed (rest : ...i64) : i64 { return rest.len; }
fn typed32 (rest : ...i32) : i64 { return rest.len; }
fn fixed (a : i64, rest : ...i64) : i64 { return a + rest.len; }

fn run (s : i64[]) : i64 {
	typed(s...);
	fixed(s...);
	typed(s..., 1);
	typed(1, s...);
	typed(rest = s...);
	typed32(s...);
	plain(s...);
	printf("%d", s...);
	return typed(s...);
}

pub fn main () : i64 { return run(ga); }

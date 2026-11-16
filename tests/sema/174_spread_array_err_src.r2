fn typed (rest : ...i32) : i64 { return rest.len; }
fn typed64 (rest : ...i64) : i64 { return rest.len; }

fn run () : i64 {
	arr x : i32[3] = {1, 2, 3};
	arr m : i32[2][3] = {{1, 2, 3}, {4, 5, 6}};

	typed64(x...);
	typed(m...);
	return typed(x...);
}

pub fn main () : i64 { return run(); }

arr ga : i32[3] = {1, 2, 3};

fn add (rest : ...i32) : i32 {
	var t : i32 = 0;

	for (var i : rest) t += rest[i];
	return t;
}

fn fwd (a : i32[3]) : i32 { return add(a...); }

pub fn main () : i64 { return add(ga...) + fwd(ga); }

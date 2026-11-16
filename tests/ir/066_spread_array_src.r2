var ga : i64[3] = {1, 2, 3};

fn sum (rest : ...i64) : i64 {
	var t : i64 = 0;

	for (var i : rest) t += rest[i];
	return t;
}

fn fwd (a : i64[3]) : i64 { return sum(a...); }

pub fn main () : i64 { return sum(ga...) + fwd(ga); }

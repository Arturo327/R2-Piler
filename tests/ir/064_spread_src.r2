arr ga : i64[3] = {10, 20, 12};

fn sum (rest : ...i64) : i64 {
	var total : i64 = 0;

	for (var i : rest) total += rest[i];
	return total;
}

fn forward (s : i64[]) : i64 { return sum(s...); }
fn make () : i64[] { return ga; }

pub fn main () : i64 { return forward(ga) + sum(make()...); }

var ga : i64[3] = {1, 2, 3};

fn sum (rest : ...i64) : i64 {
	var total : i64 = 0;

	for (var i : rest) total += rest[i];
	return total;
}

fn forward (s : i64[]) : i64 { return sum(s...); }
fn make () : i64[] { return ga; }

pub fn main () : i64 {
	arr vals : i64[4] = {1, 2, 3, 4};

	if (forward(vals) != 10) return 1;
	if (sum(make()...) != 6) return 2;
	if (sum() != 0) return 3;
	if (sum(5, 6) != 11) return 4;
	return 42;
}

fn sum (rest : ...i64) : i64 {
	var total : i64 = 0;

	for (var i : rest) total += rest[i];
	return total;
}

fn total (rest : ...any) : i64 {
	var total : i64 = 0;

	for (var i : rest) total += rest[i] as i64;
	return total;
}

fn fwd (s : any[]) : i64 { return total(s...); }

pub fn main () : i64 {
	arr v : any[2] = {1, 2};

	return sum(1, 2) + fwd(v);
}

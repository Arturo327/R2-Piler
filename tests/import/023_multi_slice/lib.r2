pub struct Pair { a : i64; b : i64; }

pub fn pair (a : i64, b : i64) : Pair
{
	var p : Pair = {.a = a, .b = b};
	return p;
}

pub fn pair2 (a : i64, b : i64) : i64, i64 { return a, b; }

pub fn sum (s : i64[]) : i64
{
	var total : i64 = 0;
	for (var i : s) { total += s[i]; }
	return total;
}

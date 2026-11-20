pub fn with_default (a : i64, b : i64 = 40) : i64 { return a + b; }

pub fn sum_all (first : i64, rest : ...i64) : i64
{
	var s : i64 = first;
	for (var i : rest) { s += rest[i]; }
	return s;
}

pub fn bump (x : &i64) : void { x += 1; }

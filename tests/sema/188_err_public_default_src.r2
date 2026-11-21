var g : i64;

pub fn exposed (p : ptr i64 = $g) : i64
{
	return @p;
}

pub fn main () : i64
{
	return 0;
}

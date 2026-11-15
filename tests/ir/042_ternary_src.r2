fn pick(c : i64, a : i64, b : i64) : i64 {
	return c ? a : b;
}

pub fn main() : i64 {
	return pick(1, 2, 3);
}
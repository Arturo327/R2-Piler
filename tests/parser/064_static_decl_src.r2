fn main() : i64 {
	static var a : i64 = 1, b : i64 = 2;
	static arr s : i8[4] = "abc";

	{
		static var c : i64;
		c = 1;
	}
	return a + b;
}
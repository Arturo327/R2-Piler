var g : i64 = 7;

fn counter() : i64 {
	static var x : i64 = 0;

	x = x + 1;
	return x;
}

fn zero() : i64 {
	static var z : i64;

	return z;
}

fn bumpz() : i64 {
	static var z : i64 = 5;

	z = z + 1;
	return z;
}

fn other() : i64 {
	static var x : i64 = 100;

	x = x + 1;
	return x;
}

fn arrsum() : i64 {
	static arr a : i64[3] = {1, 2, 3};

	a[0] = a[0] + 1;
	return a[0] + a[1] + a[2];
}

fn ptrstatic() : i64 {
	static var p : ptr i64 = $g;

	return @p;
}

fn nullstatic() : i64 {
	static var q : ptr i64 = NULL;

	if (q != NULL) { return 1; }
	return 0;
}

fn blockstat() : i64 {
	var r : i64;

	{
		static var y : i64 = 10;
		y = y + 1;
		r = y;
	}
	return r;
}

fn addrstat() : i64 {
	static var s : i64 = 30;
	var p : ptr i64 = $s;

	@p = @p + 1;
	return @p;
}

fn strstat() : i64 {
	static arr s : i8[4] = "abc";

	s[0] = 65;
	var r : i64 = s[0];

	r = r + s[1];
	return r;
}

fn main() : i64 {
	if (counter() != 1) { return 1; }
	if (counter() != 2) { return 2; }
	if (counter() != 3) { return 3; }
	if (zero() != 0) { return 4; }
	if (bumpz() != 6) { return 5; }
	if (bumpz() != 7) { return 6; }
	if (other() != 101) { return 7; }
	if (other() != 102) { return 8; }
	if (arrsum() != 7) { return 9; }
	if (arrsum() != 8) { return 10; }
	if (ptrstatic() != 7) { return 11; }
	if (nullstatic() != 0) { return 12; }
	if (blockstat() != 11) { return 13; }
	if (blockstat() != 12) { return 14; }
	if (addrstat() != 31) { return 15; }
	if (addrstat() != 32) { return 16; }
	if (strstat() != 163) { return 17; }
	if (strstat() != 163) { return 18; }
	return 0;
}
fn one () : i32 { return 1; }

pub fn main () : i64
{
	var p : ptr fn() : i32 = one;
	var v : ptr void = p;
	var q : ptr fn() : i32 = v;
	var acc : i64 = 0;

	if (q == one) acc += 10;
	if (p != NULL) acc += 20;
	if (v == NULL) acc += 40;
	acc += q() as i64;
	acc += sizeof(ptr fn() : i32) as i64;
	return acc;
}

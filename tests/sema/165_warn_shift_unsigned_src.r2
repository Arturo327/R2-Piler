// The hardware masks shift counts, so an out-of-range literal count is almost
// always a bug. Comparing an unsigned value with 0 is decided statically.

pub fn main () : i64 {
	var u : u64 = 3u;
	var small : u8 = 7;
	var wide : i64 = 0;

	if (u < 0) wide = 1;
	if (u >= 0) wide = wide + 1;
	if (0 > u) wide = 2;
	if (0 >= u) wide = 3;
	if (u > 0) wide = wide + 4;
	if (u <= 0) wide = wide + 5;

	var ok_shift : i64 = wide << 63;
	var bad_width : u8 = small << 8;
	var bad_neg : i64 = wide << -1;
	var bad_lit : i64 = 1 << 64;
	return ok_shift + (bad_width as i64) + bad_neg + bad_lit;
}
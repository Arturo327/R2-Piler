// OPT: compound assignment and postfix ++/--.
// `+= -= *= /= %= &= |= ^= <<= >>=` must behave exactly like
// `x = x <op> y` (same precedence, narrow wrap, signed shifts);
// `i++`/`i--` are statement-only and also allowed in a `for` update.
var g : i64 = 100;
var h : u8 = 250;

fn bump(v : i64) : i64 {
	v += 10;
	v *= 2;
	return v;
}

fn main() : i64 {
	var x : i64 = 10;
	x += 5;
	x -= 3;
	x *= 4;
	x /= 5;
	x %= 4;
	if (x != 1) {
		return 1;
	}

	var b : i64 = 12;
	b &= 10;
	b |= 3;
	b ^= 6;
	b <<= 2;
	b >>= 1;
	if (b != 26) {
		return 2;
	}

	var p : i64 = 1;
	var q : i64 = 2;
	p += q += 3;
	if (p != 6 || q != 5) {
		return 3;
	}

	var i : i64 = 0;
	i++;
	i++;
	i++;
	i--;
	if (i != 2) {
		return 4;
	}

	g += 7;
	g *= 2;
	g--;
	if (g != 213) {
		return 5;
	}

	h += 10;
	if (h != 4) {
		return 6;
	}
	h++;
	if (h != 5) {
		return 7;
	}

	var n : i8 = 127;
	n++;
	if (n != -128) {
		return 8;
	}
	n--;
	if (n != 127) {
		return 9;
	}

	var m : u16 = 65535;
	m += 2;
	if (m != 1) {
		return 10;
	}

	var s : i64 = -8;
	s >>= 1;
	if (s != -4) {
		return 11;
	}
	var u : u64 = 8;
	u >>= 1;
	if (u != 4) {
		return 12;
	}
	var sh : i8 = -128;
	sh >>= 1;
	if (sh != -64) {
		return 13;
	}

	var sum : i64 = 0;
	for (var k : i64 = 0; k < 5; k++) {
		sum += k;
	}
	if (sum != 10) {
		return 14;
	}
	for (; sum > 7; sum--) {
	}
	if (sum != 7) {
		return 15;
	}

	var t : i64 = 1;
	t += 1 << 2;
	if (t != 5) {
		return 16;
	}
	t *= 2 + 3;
	if (t != 25) {
		return 17;
	}
	t -= 2 * 3;
	if (t != 19) {
		return 18;
	}

	var v : i64 = 0;
	v = (x += 4);
	if (v != 5 || x != 5) {
		return 19;
	}

	if (bump(5) != 30) {
		return 20;
	}

	return 0;
}

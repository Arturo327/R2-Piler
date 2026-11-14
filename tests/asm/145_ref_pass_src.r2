fn bump2(x : &i64) {
	x += 1;
}

fn bump(x : &i64) {
	bump2(x);
}

fn getp(x : &i64) : ptr i64 {
	return $x;
}

fn advance(p : &ptr i64) {
	p = p + 1;
}

fn main() : i64 {
	var n : i64 = 10;
	bump(n);
	if (n != 11) return 1;
	if (getp(n) != $n) return 2;
	arr vals : i64[4] = {1, 2, 3, 4};
	var q : ptr i64 = $vals[0];
	advance(q);
	if (q != $vals[1]) return 3;
	if (@q != 2) return 4;
	return 0;
}

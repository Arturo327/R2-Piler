fn inc(x : &i64, d : i64) {
	x += d;
}

fn get(x : &i64) : i64 {
	return x;
}

fn addr(x : &i64) : ptr i64 {
	return $x;
}

fn main() : i64 {
	var n : i64 = 3;
	inc(n, 4);
	var m : i64 = get(n);
	return m + (addr(n) != $n);
}

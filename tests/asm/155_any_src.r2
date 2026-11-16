struct Box { v : any; }

fn kind (a : any) : i64 {
	if (a.tag == "i64") return 1;
	if (a.tag == "i8") return 2;
	if (a.tag == "u8") return 3;
	if (a.tag == "string") return 4;
	if (a.tag == "nil") return 5;
	return 0;
}

fn total (rest : ...any) : i64 {
	var total : i64 = 0;

	for (var i : rest) total += rest[i] as i64;
	return total;
}

fn fwd (s : any[]) : i64 { return total(s...); }

pub fn main () : i64 {
	var a : any = 41;
	var n : i8 = -2;
	var b : any = n;
	var box : Box = {.v = 3};
	var u : u8 = 7;
	arr vals : any[2] = {5, 6};

	if (kind(a) != 1) return 1;
	if ((a as i64) != 41) return 2;
	if (kind(b) != 2) return 3;
	if (b.value != -2) return 4;
	if ((box.v as i64) != 3) return 5;
	if (kind(u) != 3) return 6;
	if (kind("hi") != 4) return 7;
	if (kind(NULL) != 5) return 8;
	if (total(1, 2) != 3) return 9;
	if (fwd(vals) != 11) return 10;
	return 42;
}

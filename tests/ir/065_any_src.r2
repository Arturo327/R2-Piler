fn first (rest : ...any) : i64 { return rest[0] as i64; }

fn tag (a : any) : i64 {
	if (a.tag == "i64") return 1;
	return 0;
}

pub fn main () : i64 {
	var a : any = 5;

	if (tag(a) != 1) return 1;
	a = 7;
	return first(a) + (a as i64);
}

fn add (rest : ...i32) : i32 {
	var t : i32 = 0;

	for (var i : rest) t += rest[i];
	return t;
}

fn total (rest : ...any) : i64 {
	var t : i64 = 0;

	for (var i : rest) t += rest[i] as i64;
	return t;
}

fn fwd (a : i32[3]) : i32 { return add(a...); }

arr gx : i32[3] = {10, 20, 12};

pub fn main () : i64 {
	arr x : i32[5] = {1, 2, 3, 4, 5};
	arr m : i32[2][3] = {{1, 2, 3}, {4, 5, 6}};
	arr v : any[2] = {3, 4};

	if (add(x...) != 15) return 1;
	if (add(gx...) != 42) return 2;
	if (add(m[1]...) != 15) return 3;
	if (total(v...) != 7) return 4;
	if (fwd(gx) != 42) return 5;
	return 42;
}

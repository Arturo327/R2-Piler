arr g : i32[4] = {1, 2, 3, 4};
arr gm : i32[2][3] = {{1, 2, 3}, {4, 5, 6}};

pub fn main() : i64 {
	var v : i32[4] = g;
	var m : i32[2][] = gm;

	v[1] = 9;
	m[1][2] = 7;
	return (v[1] + m[1][2]) as i64;
}

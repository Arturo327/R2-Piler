arr g : i32[3] = {1, 2, 3};
arr s : i8[8] = "hi";

fn sum(v : i32[]) : i64 {
	return v.len + v[0];
}

fn main() : i64 {
	arr a : i32[2] = {4, 5};
	var n : i64 = a.len;
	return sum(a) + n;
}

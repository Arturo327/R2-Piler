fn val() : i32 {
	return 1;
}

fn main() : i64 {
	arr a : i32[2] = {1};
	arr b : i32[2] = {1, 2, 3};
	arr c : i32[2] = 5;
	arr d : i32[2] = val();
	arr e : i32[2] = {"a", 1};
	arr f : i32[2] = {{1, 2}, {3, 4}};
	arr g : i8[2] = "abc";
	arr h : i16[4] = "abc";
	arr i : i8[2] = {1, 300};
	arr j : u8[3] = {-1, 0, 0};
	var k : i32 = 1;
	return a[0];
}

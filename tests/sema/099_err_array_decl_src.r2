fn arr_param(a : i32[2]) : i64 {
	return 0;
}

fn arr_ret() : i32[] {
	return 0;
}

extern fn ext(v : i32[2], s : i32[]) : void;

fn main() : i64 {
	var x : i32[3];
	arr y : i64;
	arr z : i32[];
	arr m : i32[2][3];
	arr ok : i32[2] = {1, 2};
	return ok[0];
}

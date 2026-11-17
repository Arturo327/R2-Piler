arr _a3 : i32[3] = {1, 2, 3};
arr _a4 : i32[4] = {1, 2, 3, 4};

var _gv : i32[3];

pub fn main() : i64 {
	var _v : i32[4] = _a3;
	var _w : i32[3] = {1, 2, 3};
	var _s : i8[4] = "hi";
	var _n : i32[3] = NULL;
	static var _st : i32[3];
	var _u : i32[3] = _a3;
	_u = _a4;
	return 0;
}

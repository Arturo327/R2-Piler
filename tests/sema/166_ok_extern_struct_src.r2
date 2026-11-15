// A struct reachable from C through an extern/export pointer keeps its layout
// public: its fields are not reported as unused.

struct Foreign {
	used : i64;
	filled_by_c : i64;
}

struct ByValue {
	also_c : i64;
}

extern fn init_foreign (p : ptr Foreign) : void;
export fn touch (_p : ptr ByValue) : void {}

pub fn main () : i64 {
	var f : Foreign = {.used = 1};
	init_foreign($f);
	return f.used;
}
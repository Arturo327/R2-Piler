export var g : const i64 = 3;
export var n : i64 = 4;
export var cp : const ptr i64 = $n;
export var pc : ptr const i64 = NULL;

export struct C {
	f : const i64;
	items : const i32[2];
	p : ptr const i64;
	q : const ptr i64;
};

export fn take (_x : const i32, p : ptr const i64) : ptr const i64 { return p; }

export fn get () : const i64 { return g; }

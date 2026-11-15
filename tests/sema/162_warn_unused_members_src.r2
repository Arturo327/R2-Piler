// Unused struct fields and enum variants. Any mention counts as a use:
// designated init, dot access, offsetof and sizeof(field). A field or
// variant never named is reported. Nested anonymous structs are flattened.

struct S {
	read : i64;
	written : i64;
	dead : i64;
}

struct Nested {
	head : i64;
	struct { x : i64; y : i64; };
}

union U {
	big : i64;
	small : u32;
}

enum E : u8 { Used, Dead }

struct Muted {
	used : i64;
	_ignored : i64;
}

enum MutedEnum : u8 { _Skipped, Taken }

struct Queries {
	a : i32;
	b : i32;
}

pub fn main () : i64 {
	var s : S = {.read = 1, .written = 2};
	var n : Nested = {.head = 1, .x = 2};
	var u : U = {.big = 3};
	var e : E = E.Used;
	var q : Queries = {.a = 4};
	var m : Muted = {.used = 6};
	var me : MutedEnum = MutedEnum.Taken;
	var off : i64 = offsetof(Queries, b);
	var size : i64 = sizeof(q.b);

	s.written = s.written + 1;
	return s.read + s.written + n.head + n.x + u.big + e as i64
			+ q.a as i64 + off + size + m.used + me as i64;
}
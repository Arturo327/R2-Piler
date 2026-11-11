struct TooWide {
	a : i8 : 9;
	b : u16 : 17;
	c : u32 : 33;
	d : u8 : 64;
}

struct NotInt {
	a : ptr u8 : 8;
	b : i64[2] : 8;
}

struct Plain {
	s : i32;
}

struct StructType {
	a : Plain : 8;
}

struct Dup {
	a : u3;
	a : u5;
}

fn main () : i64 { return 0; }

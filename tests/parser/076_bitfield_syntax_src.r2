type Byte = u8;

struct Flags {
	a : u3;
	b : i5;
	c : u8 : 8;
	d : u16 : 16;
	e : u32 : 32;
	f : u64 : 64;
	g : char : 8;
	h : Byte : 8;
	i : u8 : 1;
	j : i16 : 1;
	k : u32 : 7;
	l : u64 : 33;
	m : i32 : 11;
}

union Narrow {
	whole : u8;
	part : u5;
}

struct Nested {
	head : u2;
	struct { lo : u4; hi : u4; };
	tail : u3;
}

struct Bad {
	a : u0;
	b : i65;
	c : u8 : 0;
	d : u8 : width;
}

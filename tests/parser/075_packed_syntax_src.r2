packed struct Header {
	tag : u8;
	value : u32;
}

packed union Bits {
	whole : u32;
	parts : u8[4];
}

packed struct Nested {
	head : u8;
	struct { lo : u4; hi : u4; };
	tail : u8 : 3;
}

struct Inner {
	p : Header;
	k : u8;
}

packed struct Bad {
	a : u8;
	packed struct { b : u8; };
}

packed var g : i64 = 0;

fn main () : i64 {
	return 0;
}

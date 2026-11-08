union Number {
	i: i64;
	pair: i32[2];
	bytes: u8[8];
}

struct Cpu {
	tag: i8;
	union {
		whole: u32;
		struct {
			lo: u16;
			hi: u16;
		}
	}
}

fn main() : i64 {
	var n : Number = {.i = 0};

	n.pair[0] = 1;
	n.pair[1] = 2;
	if (n.i != 0x0000000200000001) return 1;
	if (n.bytes[0] != 1) return 2;
	if (n.bytes[4] != 2) return 3;

	var c : Cpu = {.tag = 1, .whole = 0x00020001};

	if (c.lo != 1) return 4;
	if (c.hi != 2) return 5;

	c.lo = 3;
	if (c.whole != 0x00020003) return 6;
	if (c.tag != 1) return 7;

	return 0;
}

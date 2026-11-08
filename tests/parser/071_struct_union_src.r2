struct Punto {
	x: i32;
	y: i32;
	tags: u16[3];
}

union Dato {
	entero: i64;
	corto: i32;
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

fn suma(p : Punto) : i32 {
	return p.x + p.y;
}

fn main() : i64 {
	var p : Punto = {.x = 1, .y = 2, .tags = {3, 4, 5}};
	var c : Cpu = {.tag = 0, .whole = 7};
	var llamada : i32 = suma(p);

	p.x = p.y;
	c.lo = 9;
	return llamada as i64;
}

class Box {
	v : i64;

	init (self, v : i64) { self.v = v; }

	op = (self, b : Box) : void { self.v = b.v + 1; }
}

class Plain {
	v : i64;

	init (self, v : i64) { self.v = v; }
}

struct Holder { b : Box; }

var g : Box = Box(10);
var h : Box = g;

fn make(v : i64) : Box { return Box(v); }

pub fn main() : i64
{
	var one : Box = Box(1);
	var two : Box = one;
	var three : Box = two;
	var plain : Plain = Plain(7);
	var plain2 : Plain = plain;
	var sum : i64 = one.v + two.v + three.v + plain.v + plain2.v;
	var s : Holder = {.b = Box(0)};
	var four : Box = make(3);

	three = two;
	s.b = three;
	arr list : Box[2];
	list[1] = s.b;
	sum += three.v + s.b.v + list[1].v;
	g = list[1];
	plain2 = plain;
	return sum + g.v + h.v + plain2.v + four.v;
}

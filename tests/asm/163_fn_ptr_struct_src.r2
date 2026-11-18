struct Box {
	op : ptr fn() : i32;
	n : i32;
}

fn forty () : i32 { return 40; }

fn add_n (b : ptr Box) : i32
{
	return b.op() + b.n;
}

pub fn main () : i64
{
	var b : Box = {.op = forty, .n = 2};
	var copy : Box = b;
	var p : ptr Box = $copy;

	return (b.op() + p.op() + add_n(p)) as i64;
}

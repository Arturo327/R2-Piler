import "libb";

pub fn main () : i64
{
	var p = libb::wrap(5);
	return p.a + p.b;
}

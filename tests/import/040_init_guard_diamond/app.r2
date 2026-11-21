import a;
import b;
import c;

pub fn main () : i64
{
	if (a::get() != 41) return 1;
	if (b::get() != 41 || c::get() != 41) return 2;
	if (a::calls() != 1) return 3;
	return 0;
}

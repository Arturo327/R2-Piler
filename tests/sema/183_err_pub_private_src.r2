struct Hidden {
	_x : i64;
}

pub struct Exposed {
	h : Hidden;
}

pub fn expose (p : ptr Hidden) : Hidden
{
	return @p;
}

pub fn main () : i64
{
	return 0;
}
struct _Hidden {
	x : i64;
}

fn helper () : i64
{
	return 1;
}

pub fn main () : i64
{
	return helper();
}
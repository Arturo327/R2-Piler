from lib import val;

fn helper () : i64 { return val(); }

pub fn main () : i64
{
	var val : i64 = 5;
	return val + helper();
}

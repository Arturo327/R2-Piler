fn suma (a : i32, b : i32) : i32 { return a + b; }

fn call_twice (x : i32) : i32
{
	static var cb : ptr fn(a : i32, b : i32) : i32 = suma;

	return cb(x, x);
}

pub fn main () : i64
{
	return call_twice(3) as i64 + call_twice(4) as i64;
}

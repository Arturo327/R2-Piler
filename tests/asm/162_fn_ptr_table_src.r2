fn suma (a : i32, b : i32) : i32 { return a + b; }
fn resta (a : i32, b : i32) : i32 { return a - b; }
fn mul (a : i32, b : i32) : i32 { return a * b; }

pub fn main () : i64
{
	arr ops : (ptr fn(a : i32, b : i32) : i32)[3] = {suma, resta, mul};
	arr vals : i32[2] = {8, 3};
	var acc : i32 = 0;

	for (var i : i64 = 0; i < 3; i++)
		acc += ops[i](vals[0], vals[1]);
	return acc as i64;
}

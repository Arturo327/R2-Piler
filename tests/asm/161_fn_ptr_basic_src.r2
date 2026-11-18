fn suma (a : i32, b : i32) : i32
{
	return a + b;
}

fn resta (a : i32, b : i32) : i32
{
	return a - b;
}

pub fn main () : i64
{
	var callback : ptr fn(a : i32, b : i32) : i32 = suma;
	var acc : i32 = callback(4, 5);

	callback = resta;
	acc += callback(9, 4);
	return acc as i64;
}

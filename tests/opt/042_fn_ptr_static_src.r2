type Op = ptr fn(a : i32, b : i32) : i32;

fn suma (a : i32, b : i32) : i32 { return a + b; }
fn resta (a : i32, b : i32) : i32 { return a - b; }

var g : Op = suma;

pub fn main () : i64
{
	var local : Op = resta;
	g = suma;
	return local(1, 2) as i64 + g(3, 4) as i64;
}

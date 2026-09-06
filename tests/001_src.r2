fn sumar (a:i64, b:i64) : i64
{
	var c : i64;
	c = a + b;
	return c;
}

fn mult (a:i64, b:i64) : i64
{
	var result : i64;
	result = 0;

	var i : i64;
	for (i = 0; i < b; i = i + 1) {
		result = result + a;
	}

	return result;
}

fn main ()
{
	var letra : char;
	letra = 'a';

	var a:i64 = 7;
	var b:i64 = 3;

	var c:i64 = sumar(a, b);
	var d:i64 = mult(a, b);
}

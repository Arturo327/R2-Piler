fn id (x : i64) : i64
{
	return x;
}

fn sum10 (a : i64, b : i64, c : i64, d : i64, e : i64, f : i64, g : i64, h : i64, i : i64, j : i64) : i64
{
	return a + b + c + d + e + f + g + h + i + j;
}

fn main () : i64
{
	var a : i64 = id(1);
	var b : i64 = id(2);
	var c : i64 = id(3);
	var d : i64 = id(4);
	var e : i64 = id(5);
	var f : i64 = id(6);
	var g : i64 = id(7);
	var h : i64 = id(8);
	var i : i64 = id(9);
	var j : i64 = id(10);

	return sum10(a, b, c, d, e, f, g, h, i, j);
}

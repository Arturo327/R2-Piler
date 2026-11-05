fn main () : i64
{
	arr lista : i32[3] = { 1, 2, 3 };
	arr big : i32[300];
	var x : i64 = 7;
	var t : i64 = 0;

	for (var i : i64 : x)
		t += i;

	for (var j : i8 : big)
		t += j as i64;

	for (nada : lista)
		t += nada;

	return t + x;
}

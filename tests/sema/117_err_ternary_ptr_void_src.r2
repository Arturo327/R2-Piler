fn main() : i64 {
	arr a : i64[2] = {1, 2};
	var p : ptr i64 = a;
	var v : ptr void = p;
	var c : i64 = 0;
	var ok1 : ptr void = c ? p : v;
	var ok2 : ptr void = c ? v : p;
	var x : i64 = (c ? p : v)[0];
	var y : i64 = (c ? v : p)[0];

	return ok1 == NULL;
}

arr glob : i32[4] = { 1, 2, 3, 4 };
arr mat : i32[2][3] = { { 1, 2, 3 }, { 4, 5, 6 } };

fn suma (v : i32[]) : i32
{
	var total : i32 = 0;

	for (var i : i64 : v)
		total += v[i];

	return total;
}

fn main () : i64
{
	arr local : i32[5] = { 10, 20, 30, 40, 50 };
	var t : i64 = 0;
	var i : i64;

	for (var a : i64 : local)
		t += local[a] as i64;

	for (var b : local)
		t += local[b] as i64;

	for (i : glob)
		t += glob[i] as i64;

	for (i : mat) {
		for (var j : mat[i])
			t += mat[i][j] as i64;
	}

	t += suma(local) as i64;

	if (t != 481) { return 1; }
	return 0;
}

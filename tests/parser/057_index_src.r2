fn main () : i64 {
	var i : i64 = 1;
	var p : ptr i64 = NULL;
	var pp : ptr ptr i64 = NULL;
	var a : i64 = p[0] + p[i] + p[i + 1];
	var b : i64 = pp[i][0];
	p[i] = a;
	p[i] += b;
	p[i]++;
	var c : ptr i64 = $p[i];
	return a + b;
}

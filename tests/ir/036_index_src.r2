var g : i64 = 10;

fn main () : i64 {
	var p : ptr i64 = $g;
	p[1] = 3;
	var x : i64 = p[0] + @(p + 1);
	p[1] += x;
	p[2]++;
	return x;
}

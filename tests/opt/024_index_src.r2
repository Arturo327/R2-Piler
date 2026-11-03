var g : i64 = 10;

fn main () : i64 {
	var p : ptr i64 = $g;
	p[0] = 3;
	p[1] = p[0] + 4;
	return p[1];
}

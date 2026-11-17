arr _g : i32[6] = {1, 2, 3, 4, 5, 6};
arr _gm : i32[2][3] = {{1, 2, 3}, {4, 5, 6}};
arr _g2 : i32[3][2] = {{1, 2}, {3, 4}, {5, 6}};

fn _take(a : i32[6]) : i64 { return a[0] as i64; }
fn _take_slice(a : i32[]) : i64 { return a[0] as i64; }
fn _take_mixed(a : i32[2][]) : i64 { return a[0][0] as i64; }

pub fn main() : i64 {
	arr local : i32[6] = {7, 8, 9, 10, 11, 12};
	var a : i32[6] = local;
	var b : i32[6] = _g;
	var c : i32[6];
	var v : i32[2][3] = _gm;
	var x : i32[2][] = _gm;
	var y : i32[3][] = _g2;
	var n : i64 = a.len + v[0].len + x[0].len + y[0].len;
	var s : i64 = sizeof(a) + sizeof(x) + sizeof(y);
	var t : i64 = _take(a) + _take_slice(b) + _take_mixed(x);
	var p : ptr i32 = v;

	c = a;
	var c0 : i64 = c[3];
	c = b;
	c[0] = c[1];
	v[1][2] = 9;
	x = _gm;
	var x0 : i64 = x[0][0];
	y = _g2;
	var y0 : i64 = y[0][0];
	return n + s + t + (p[0] as i64) + c0 + x0 + y0;
}

var g : i64 = 5;

fn bad (x : i64 = 1, y : i64) : i64 { return x + y; }
fn opt (x : i64, y : i64 = 2) : i64 { return x + y; }
fn req (x : i64, y : i64) : i64 { return x + y; }
fn noconst (x : i64 = g) : i64 { return x; }

fn main () : i64 {
	var a : i64 = opt(1, z = 3);
	var b : i64 = opt(1, y = 3, y = 4);
	var c : i64 = opt(y = 1, 2);
	var d : i64 = req(y = 1);
	var e : i64 = opt(1, 2, 3);
	var f : i64 = noconst();
	var h : i64 = bad(1, 2);
	return a + b + c + d + e + f + h;
}
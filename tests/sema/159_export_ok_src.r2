export fn cb (x : i64) : i64 { return x + 1; }
export var g : i64 = 7;
export arr a : i64[2] = {1, 2};

pub fn main () : i64 {
	return cb(g) + a[0];
}

import "stdlib";

pub fn main() : i64 {
	var x : (ptr i64)[] = new ptr i64[2];
	x[0] = NULL;
	x[1] = NULL;
	return x.len as i64;
}

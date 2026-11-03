fn main () : i64 {
	var p : ptr i64 = NULL;
	var s : ptr i8 = "x";
	return p[s] as i64;
}

fn f(n : i64) : void {
	var a : i32[] = new i32[n];
	var b : i32[][] = new i32[2][n];
	var c : ptr i32 = new i32;
	var d : i32[] = new(mi_alloc(16)) i32[4];
	delete a;
	delete(b);
	delete(free_it) d;
}

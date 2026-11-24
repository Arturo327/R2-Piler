extern fn malloc (n : u64) : ptr void;
extern fn free (p : ptr void) : void;

var freed : i64 = 0;
var released : i64 = 0;
var gpool : i32[] = new(malloc(40)) i32[10];

fn counted_free (p : ptr void) : i32 {
	free(p);
	freed += 1;
	return 0;
}

fn plain_free (p : ptr void) : void {
	free(p);
	released += 1;
}

fn bump (n : i64) : i64 {
	static var sp : ptr i32 = new(malloc(4)) i32;

	@sp += 1;
	return (@sp as i64) + n;
}

pub fn main () : i64 {
	var b : i32[] = new(malloc(4000)) i32[1000];
	var i : i64 = 0;

	while (i < 1000) { b[i] = 1; i++; }
	if (b.len != 1000) return 1;
	var s : i64 = 0;

	i = 0;
	while (i < 1000) { s += b[i] as i64; i++; }
	if (s != 1000) return 2;

	var p : ptr i32 = new(malloc(4)) i32;

	@p = 9;
	if (@p != 9) return 3;

	gpool[3] = 5;
	if (gpool.len != 10 || gpool[3] != 5) return 6;
	if (bump(0) != 1) return 7;
	if (bump(0) != 2) return 8;

	var d : ptr fn(p : ptr void) : void = plain_free;

	delete(counted_free) b;
	delete(d) p;
	delete(d) NULL;
	if (freed != 1) return 4;
	if (released != 2) return 5;
	return 0;
}

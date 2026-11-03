extern fn malloc (size : u64) : ptr i8;
extern fn free (p : ptr i8) : void;

fn main () : i64 {
	var raw : ptr i8 = malloc(40);
	var p : ptr i64 = raw as ptr i64;
	var i : i64 = 0;

	while (i < 5) {
		p[i] = i + 1;
		i += 1;
	}

	p[0] += 10;
	p[1]++;
	var q : ptr i64 = $p[2];
	@q = 7;

	var r : i64 = p[0] + p[1] + p[2] + p[3] + p[4];
	if (p[3] != @(p + 3)) { free(raw); return 99; }
	if ($p[4] != p + 4) { free(raw); return 98; }
	free(raw);
	return r;
}

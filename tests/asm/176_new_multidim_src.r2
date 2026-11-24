import "stdlib";

pub fn main () : i64 {
	var m : i32[][] = new i32[10][10];

	for (var i : i64, j : i64 : m) { m[i][j] = (i * 10 + j) as i32; }
	if (m.len != 10) return 1;
	if (m[0].len != 10) return 2;
	if (m[9][9] != 99) return 3;

	var sum : i64 = 0;

	for (var i : i64, j : i64 : m) { sum += m[i][j] as i64; }
	var expect : i64 = 0;
	var a : i64 = 0;

	while (a < 10) {
		var b : i64 = 0;
		while (b < 10) { expect += a * 10 + b; b++; }
		a++;
	}
	if (sum != expect) return 4;

	var cube : i32[][][] = new i32[2][3][4];

	for (var i : i64, j : i64, k : i64 : cube) { cube[i][j][k] = 1; }
	if (cube[0][0].len != 4) return 5;

	var count : i64 = 0;

	for (var i : i64, j : i64, k : i64 : cube) { count += cube[i][j][k] as i64; }
	if (count != 24) return 6;

	var row : ptr i32 = cube[1][2];

	@row = 7;
	if (cube[1][2][0] != 7) return 7;
	delete m;
	delete cube;
	return 0;
}

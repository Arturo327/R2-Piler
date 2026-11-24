fn probe (m : i32[][][]) : i64 {
	var l : i32[][][] = m;

	return l[0].len * 100 + l[0][1].len * 10 + l[2][1].len;
}

fn probe2 (m : i32[][][]) : i64 {
	var l : i32[][][] = m;
	var total : i64 = 0;
	var i : i64 = 0;

	while (i < l.len) {
		total += l[i].len * 100 + l[i][0].len * 10 + l[i][1].len;
		i++;
	}
	return total;
}

pub fn main () : i64 {
	arr x : i32[3][2][4];
	var i : i64 = 0;

	while (i < 3) {
		var j : i64 = 0;
		while (j < 2) {
			var k : i64 = 0;
			while (k < 4) { x[i][j][k] = (i * 100 + j * 10 + k) as i32; k++; }
			j++;
		}
		i++;
	}
	if (probe(x) != 244) return 1;
	if (probe2(x) != 732) return 2;
	if (x[2][1][3] != 213) return 3;
	return 0;
}

fn take_u8(x : u8) : u8 { return x; }
fn take_i32(x : i32) : i32 { return x; }

fn narrows(v : i32[]) : i64 {
	var a : u8 = v.len;
	var b : i8 = v.len;
	var c : i16 = v.len;
	var d : u16 = v.len;
	var e : i32 = v.len;
	var f : i64 = v.len;

	return (a as i64) + (b as i64) + (c as i64) + (d as i64) + (e as i64) + f;
}

fn passes(v : i32[]) : i64 {
	var a : u8 = take_u8(v.len);
	var b : i32 = take_i32(v.len);

	return (a as i64) + (b as i64);
}

fn returns(v : i32[]) : i16 { return v.len; }

fn mixed(v : i32[], n : i32) : i32 { return v.len + n; }

fn loops(v : i32[]) : i64 {
	var i : i32 = 0;

	while (i < v.len) {
		i++;
	}
	return i as i64;
}

fn indexes(v : i32[]) : i64 { return v[v.len - 1] as i64; }

fn main() : i64 {
	arr big : i32[300];
	arr small : i32[3] = {10, 20, 30};
	var t : i64 = 0;

	t += narrows(big);
	t += narrows(small);
	t += passes(small);
	t += returns(small) as i64;
	t += mixed(big, 7) as i64;
	t += loops(small);
	t += indexes(small);
	if (t != 1655) { return 1; }
	return 0;
}
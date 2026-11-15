struct P { n : i64; name : char[]; }
var g : char[] = "global";

fn take(s : char[]) : i64 { return s.len; }
fn give() : char[] { return "returned"; }

pub fn main() : i64 {
	static var sg : char[] = "static!";
	var s : char[] = "Hello";
	var p : P = {.n = 1, .name = "field"};
	var q : P = {.n = 2, .name = s};
	var total : i64 = take("arg") + take(s) + give().len;
	total += p.name.len + q.name.len;
	total += g.len + sg.len + g[0] as i64;
	if (p.name[0] != 'f') return 1;
	if (give()[0] != 'r') return 2;
	if (sg[3] != 't') return 3;
	return total;
}
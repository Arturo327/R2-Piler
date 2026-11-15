struct P { n : i64; name : char[]; }
var g : char[] = "global";

fn take(s : char[]) : i64 { return s.len; }
fn give() : char[] { return "returned"; }

pub fn main() : i64 {
	static var sg : char[] = "static";
	var s : char[] = "Hello";
	var t : char[] = "abc";
	var p : P = {.n = 1, .name = "field"};
	var q : P = {.n = 2, .name = s};
	var l : i64 = s.len + sizeof(s) + take(t) + give().len;
	s = "hi";
	p.name = "other";
	return l + g.len + sg.len + p.name.len + q.name.len + s.len;
}
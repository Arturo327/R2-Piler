import "lib";

pub fn main() : i64
{
	var a : lib::Acc = lib::Acc(1);
	var b : lib::Acc = lib::Acc(2);
	var z : i64 = a.x;

	a = b;
	a += b;
	a++;
	var c : lib::Acc = a;
	var d : lib::Acc = 5 + a;	// op+ result is copied in with op= too
	var r : i64 = 0;
	if (a) { r += 1; }
	if (115 != a) { r += 2; }
	return a.x + c.x + d.x + r + z;
}

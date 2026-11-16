struct P { x : i64; }

fn take (a : any) : any { return a; }

pub fn main () : i64 {
	var a : any = 5;
	var b : any = NULL;
	var c : any = "hi";
	var d : any = a;
	var x : P = {.x = 1};
	var e : i64 = a;
	var f : any = x;
	var g : P = a as P;
	var h : i64 = b as i64;
	var i : u8 = d as u8;

	return take(c).value + x.x + h + (i as i64);
}

struct Big { a : i64; b : i64; c : i64; d : i64; e : i64; f : i64; g : i64; h : i64; i : i64; }

fn make (k : i64) : Big
{
	var r : Big = {.a = k, .b = 2, .c = 3, .d = 4, .e = 5, .f = 6, .g = 7, .h = 8, .i = 9};
	r.b = k + 1;
	return r;
}

fn main () : i64
{
	var x : Big = make(1);
	var t : i64 = x.b;
	x = make(5);
	return t * 10 + x.b;
}

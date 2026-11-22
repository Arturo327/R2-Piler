import "lib";

from "lib" import Pair as P, E, make as mk, Handle;

pub fn main () : i64
{
	var p : P = mk(4, 5);
	var e : E = E.Two;
	var id : lib::Id = 1;
	var h : Handle = 2;
	return p.a + p.b + (e as i64) + id + (h as u64 as i64);
}

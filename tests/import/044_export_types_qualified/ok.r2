import "b";
import "lib";
from "lib" import Box;

pub fn main () : i64 {
	var q : lib::Box = {.v = 9} as lib::Box;
	var r : Box = {.v = 3} as Box;
	var c : lib::Color = b::red();
	return b::boxed(q) + b::boxed(r) + b::make(2) + (c as i64);
}

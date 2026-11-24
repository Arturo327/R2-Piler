import "b";
import "lib";

pub fn main () : i64 {
	var q : Box = {.v = 1} as Box;
	var c : Color = b::red();
	return c as i64;
}

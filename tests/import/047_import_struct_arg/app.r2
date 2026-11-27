module "app";
import "big";

pub fn main() : i64
{
	var t : big::Trio = {.a = 1, .b = 2, .c = 3};
	return big::suma(t) - 6;
}

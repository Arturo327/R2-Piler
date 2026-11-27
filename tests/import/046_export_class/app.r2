module "app";
import "exp";

pub fn main() : i64
{
	var e : exp::E = exp::E(5);
	var d : exp::E = exp::E::doble();
	var r : i64 = e.get() + d.get();

	drop e;
	drop d;
	return r - 7;
}

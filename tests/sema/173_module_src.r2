module "";
module "one";
module "two";

fn f () : i64 {
	module "inner";
	return 1;
}

pub fn main () : i64 { return f(); }

module "asmmod";

var g : i64 = make(10);

fn make (x : i64) : i64 { return x + 1; }

pub fn main () : i64 { return g + 31; }

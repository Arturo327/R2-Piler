module "math2";

var g : i64 = make(5);

fn make (x : i64) : i64 { return x; }
pub fn add (a : i64, b : i64) : i64 { return a + b + g - 5; }

pub fn add (a : i64, b : i64) : i64 { return a + b; }
fn hidden () : i64 { return 1; }
pub var count : i64 = 3;
var secret : i64 = 4;
pub fn main () : i64 { return add(count, secret) + hidden(); }

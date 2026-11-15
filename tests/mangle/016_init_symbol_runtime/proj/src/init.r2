extern fn getpid () : i64;
var a : i64 = getpid();
pub fn main () : i64 { return a > 0 ? 0 : 1; }

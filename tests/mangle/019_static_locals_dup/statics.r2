fn f () : i64 {
  { static var x : i64 = 2; }
  { static var x : i64 = 3; }
  { static var x : i64 = 4; }
  return 0;
}
fn g () : i64 { static var x : i64 = 5; return x; }
pub fn main () : i64 { return f() + g(); }

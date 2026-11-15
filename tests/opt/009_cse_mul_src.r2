fn f(a : i64, b : i64) : i64 {
  var p : i64 = a * b;
  var q : i64 = a * b;
  return p + q;
}
pub fn main() : i64 {
  return f(6, 7);
}

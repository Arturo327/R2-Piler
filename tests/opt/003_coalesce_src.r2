fn f(a : i64, b : i64) : i64 {
  var s : i64 = a + b;
  var t : i64 = s;
  t = 99;
  return t;
}
fn main() : i64 {
  return f(1, 2);
}

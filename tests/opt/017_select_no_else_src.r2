fn f(c : i64) : i64 {
  var x : i64 = 0;
  if (c) {
    x = 5;
  }
  return x;
}
pub fn main() : i64 {
  return f(1);
}

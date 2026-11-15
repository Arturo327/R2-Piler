fn f(a : i64) : i64 {
  var x : i64 = a + 1;
  x = 2;
  return x;
}
pub fn main() : i64 {
  return f(10);
}

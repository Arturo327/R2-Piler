fn f(a : i64) : i64 {
  var x : i64 = a + 0;
  var y : i64 = x * 1;
  var z : i64 = y - 0;
  var w : i64 = z | 0;
  return w;
}
fn main() : i64 {
  return f(5);
}

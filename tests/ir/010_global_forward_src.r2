var x : i64 = y;
var y : i64 = 73;
var w : i64 = later(10);
fn later(a : i64) : i64 {
  return a + 1;
}
fn main() : i64 {
  return x + y + w;
}

var g : i64 = 0;
fn main() : i64 {
  var x : i64 = 1;
  var y : i64 = 2;
  x = y = 5;
  g = x + y;
  var z : i64 = (x = y);
  return g + z;
}

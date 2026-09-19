fn main() : i64 {
  var x : i64 = 0;
  var y : i64 = 0;
  x = y = 3;
  if (x = 1) {
    return x;
  }
  while (y = x) {
    y = 0;
  }
  return y;
}

fn main() : i64 {
  var a : i64 = 1;
  var b : i64 = 0;
  if (a && b) {
    return 1;
  }
  if (a || b) {
    return 2;
  }
  if (!(a && b) || !a) {
    return 3;
  }
  while (a && !b) {
    b = 1;
  }
  return b;
}

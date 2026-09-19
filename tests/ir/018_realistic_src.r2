fn fact(n : i64) : i64 {
  var r : i64 = 1;
  var i : i64 = 2;
  while (i <= n) {
    r = r * i;
    i = i + 1;
  }
  return r;
}
var start : i64 = 5;
fn main() : i64 {
  var f : i64 = fact(start);
  if (f == 120) {
    return 0;
  }
  return 1;
}

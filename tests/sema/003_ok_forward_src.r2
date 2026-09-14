var x : i64 = y;
var y : i64 = 1;
var w : i64 = later(10);
fn later(a : i64) : i64 {
  return a + 1;
}
fn fact(n : i64) : i64 {
  if (n <= 1) {
    return 1;
  } else {
    return n * fact(n - 1);
  }
}

fn f(a : i64, b : i64, c : i64) : i64 {
  var t : i64 = a + b;
  var x : i64 = t;
  if (c) {
    x = 1;
  } else {
    x = 2;
  }
  return x;
}
fn main() : i64 {
  return f(1, 2, 3);
}

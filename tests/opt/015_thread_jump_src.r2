fn f(a : i64, b : i64) : i64 {
  var x : i64;
  if (a) {
    if (b) {
      x = 1;
    } else {
      x = 2;
    }
  } else {
    x = 3;
  }
  return x;
}
fn main() : i64 {
  return f(1, 2);
}

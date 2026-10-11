fn f() : i64 {
  var x : i64 = 0;
  if (1) {
    x = 10;
  } else {
    x = 20;
  }
  return x;
}
fn main() : i64 {
  return f();
}

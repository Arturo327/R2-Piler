fn add(a : i64, b : i64) : i64 {
  return a + b;
}
fn noval() : void {
  return;
}
fn main() : i64 {
  var x : i64 = add(1, 2);
  var y : i64 = add(add(1, 2), 3);
  noval();
  add(x, y);
  return y;
}

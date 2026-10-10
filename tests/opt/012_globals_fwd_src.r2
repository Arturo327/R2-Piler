var g : i64 = 0;
fn tick() : void {
}
fn f() : i64 {
  g = 5;
  var x : i64 = g;
  return x;
}
fn h() : i64 {
  g = 5;
  tick();
  var y : i64 = g;
  return y;
}
fn main() : i64 {
  return f() + h();
}

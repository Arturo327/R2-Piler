var a : i64 = 1;
var b : i64 = a + 2;
var c : char = 'z';
var u : u64 = 7u;
fn get() : i64 {
  return a + b;
}
fn set(v : i64) : void {
  a = v;
  b = v + 1;
  return;
}
fn main() : i64 {
  set(10);
  return get();
}

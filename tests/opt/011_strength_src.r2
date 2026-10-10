fn f(a : u64) : u64 {
  var m : u64 = a * 8u;
  return m;
}
fn g(b : u64) : u64 {
  var q : u64 = b / 8u;
  return q;
}
fn h(c : u64) : u64 {
  var r : u64 = c % 8u;
  return r;
}
fn n(a : i64) : i64 {
  return a / 8;
}
fn main() : i64 {
  var s : u64 = f(16u) + g(16u) + h(16u);
  return s as i64 + n(16);
}

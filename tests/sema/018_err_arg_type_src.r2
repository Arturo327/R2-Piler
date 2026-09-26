fn add(a : i64, b : i64) : i64 {
  return a + b;
}
fn f() : i64 {
  var c : char = 'a';
  var u : u64 = 1u;
  var x : i64 = add(u, c);
  var y : i64 = add(c, u);
  return x;
}

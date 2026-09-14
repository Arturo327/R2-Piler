fn add(a : i64, b : i64) : i64 {
  return a + b;
}
fn f() : i64 {
  var x : i64 = add(1, 'a');
  var y : i64 = add(1u, 2);
  return x;
}

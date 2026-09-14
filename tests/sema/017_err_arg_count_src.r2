fn one(a : i64) : i64 {
  return a;
}
fn f() : i64 {
  var x : i64 = one();
  var y : i64 = one(1, 2);
  return x;
}

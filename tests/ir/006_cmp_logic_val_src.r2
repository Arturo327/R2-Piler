fn cmp(a : i64, b : i64) : i64 {
  var e : i64 = a == b;
  var n : i64 = a != b;
  var g : i64 = a > b;
  var ge : i64 = a >= b;
  var l : i64 = a < b;
  var le : i64 = a <= b;
  var al : i64 = a && b;
  var ol : i64 = a || b;
  var nt : i64 = !a;
  return e + n + g + ge + l + le + al + ol + nt;
}
fn main() : i64 {
  return cmp(1, 2);
}

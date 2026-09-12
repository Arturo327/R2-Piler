if (x) {
  var y : i64 = 1;
}
if (x) y = 1;
fn f() : i64 {
  if (x) return 1;
  if (x) {
    return 2;
  }
  return 0;
}
if (x) var y : i64 = 2;

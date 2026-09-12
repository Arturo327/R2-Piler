while (x) {
  x = x + 1;
}
while (x) x = x + 1;
while (x) {
}
while (x) var y : i64 = 1;
fn f() : i64 {
  while (x) return 1;
  while (x) {
    return 2;
  }
  return 0;
}

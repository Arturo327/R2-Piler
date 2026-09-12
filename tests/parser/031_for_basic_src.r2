for (i = 0; i < 10; i = i + 1) {
  x = x + i;
}
for (var i : i64 = 0; i < 10; i = i + 1) {
  x = x + i;
}
for (i = 0; i < 10; i = i + 1) x = x + 1;
for (var i : i64 = 0; i < 10; i = i + 1) x = 1;
for (i = 0; i < 10; i = i + 1) {
}
for (var i : i64; i < 10; i = i + 1) {
  x = 1;
}
fn sum(n : i64) : i64 {
  var s : i64 = 0;
  for (var i : i64 = 0; i < n; i = i + 1) {
    s = s + i;
  }
  return s;
}

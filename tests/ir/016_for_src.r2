fn main() : i64 {
  var s : i64 = 0;
  for (var i : i64 = 0; i < 5; i = i + 1) {
    s = s + i;
  }
  for (; s < 10; s = s + 1) {
    s = s + 1;
  }
  for (var k : i64 = 0; k < 3;) {
    k = k + 1;
    s = s + k;
  }
  return s;
}
fn empty() : void {
  for (;;) {
    return;
  }
  return;
}

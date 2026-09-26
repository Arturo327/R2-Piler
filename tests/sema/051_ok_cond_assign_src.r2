fn f(c : i64) : i64 {
  var a : i64;
  var b : i64;
  if (c == 1) {
    a = 1;
    b = 1;
  } elif (c == 2) {
    a = 2;
    b = 2;
  } else {
    a = 3;
    b = 3;
  }
  return a + b;
}
fn g(c : i64) : i64 {
  var w : i64 = 0;
  var s : i64 = 0;
  while (w < c) {
    w = w + 1;
  }
  for (s = 0; s < c; s = s + 1) {
  }
  return w + s;
}

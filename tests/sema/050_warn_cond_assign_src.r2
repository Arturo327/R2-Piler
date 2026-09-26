fn f(c : i64) : i64 {
  var a : i64;
  var b : i64;
  var w : i64 = 0;
  if (c == 1) {
    a = 1;
  } elif (c == 2) {
    a = 2;
    b = 2;
  } else {
    b = 3;
  }
  while (w < c) {
    w = w + 1;
  }
  return a + b + w;
}

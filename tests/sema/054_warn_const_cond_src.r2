fn f(c : i64) : i64 {
  var y : i64;
  if (1) {
    y = 1;
  }
  return y;
}
fn g(c : i64) : i64 {
  var z : i64;
  if (0) {
    z = 1;
  } else {
    z = 2;
  }
  while (0) {
    z = 3;
  }
  return z;
}
fn h(c : i64) : i64 {
  if (c == 1) {
    return 1;
  } elif (0) {
    return 2;
  } else {
    return 3;
  }
}

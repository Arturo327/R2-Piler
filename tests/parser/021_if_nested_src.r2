if (a) if (b) x = 1;
if (a) if (b) x = 1; else y = 2;
if (a) {
  if (b) {
    x = 1;
  } else {
    y = 2;
  }
} else {
  z = 3;
}
fn f(a : i64) : i64 {
  if (a > 0) {
    if (a > 10) {
      return 2;
    } else {
      return 1;
    }
  } else {
    return 0;
  }
}

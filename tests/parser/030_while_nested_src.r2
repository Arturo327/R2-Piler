while (a) while (b) x = 1;
while (a) {
  while (b) {
    x = 1;
  }
}
while (a) {
  if (b) {
    x = 1;
  } else {
    y = 2;
  }
}
if (a) {
  while (b) {
    x = 1;
  }
} else {
  while (c) {
    y = 2;
  }
}
fn f(a : i64) : i64 {
  while (a > 0) {
    if (a > 10) {
      return 2;
    } else {
      return 1;
    }
  }
  return 0;
}

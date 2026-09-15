fn v() : void {
  return;
}
fn f() : i64 {
  if (v()) {
    return 1;
  }
  return 0;
}
fn g() : void {
  while (v()) {
    return;
  }
  return;
}
fn h() : void {
  for (; v();) {
    return;
  }
  return;
}

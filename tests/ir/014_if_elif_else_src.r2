fn f(x : i64) : i64 {
  if (x == 1) {
    return 1;
  } elif (x == 2) {
    return 2;
  } elif (x == 3) {
    return 3;
  } else {
    return 4;
  }
}
fn main() : i64 {
  return f(2);
}

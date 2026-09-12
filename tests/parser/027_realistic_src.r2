fn max(a : i64, b : i64) : i64 {
  if (a > b) {
    return a;
  } else {
    return b;
  }
}
fn fact(n : i64) : i64 {
  if (n <= 1) {
    return 1;
  } else {
    return n * fact(n - 1);
  }
}
var m : i64 = max(10, 20);
var f5 : i64 = fact(5);
var flag : i64 = 0;
if (m == 20 && f5 == 120) {
  flag = 1;
} elif (m > 20) {
  flag = 2;
} else {
  flag = 3;
}

fn fact(n : i64) : i64 {
  var r : i64 = 1;
  var i : i64 = 2;
  while (i <= n) {
    r = r * i;
    i = i + 1;
  }
  return r;
}
fn sum(n : i64) : i64 {
  var s : i64 = 0;
  for (var i : i64 = 0; i < n; i = i + 1) {
    s = s + i;
  }
  return s;
}
fn max3(a : i64, b : i64, c : i64) : i64 {
  var m : i64 = a;
  if (b > m) {
    m = b;
  }
  if (c > m) {
    m = c;
  }
  return m;
}
var f5 : i64 = fact(5);
var s10 : i64 = sum(10);
var m : i64 = max3(f5, s10, 100);
var total : i64 = 0;
var k : i64 = 0;
for (k = 0; k < m; k = k + 1) {
  if (k == 50) {
    total = total + 1000;
  } elif (k > 100) {
    total = total + 2;
  } else {
    total = total + 1;
  }
}
while (total > 0) {
  total = total - 7;
}

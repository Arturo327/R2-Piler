fn f(x : i64) : i64 {
  if (x) var y : i64 = 1;
  y = 2;
  for (var i : i64 = 0; i < 10; i = i + 1) {
    var z : i64 = i;
  }
  i = 1;
  return x;
}

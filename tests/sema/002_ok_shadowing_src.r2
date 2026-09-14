var x : i64 = 1;
fn f() : i64 {
  var x : i64 = 2;
  {
    var x : i64 = 3;
    x = x + 1;
  }
  return x;
}

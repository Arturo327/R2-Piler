fn main() : i64 {
  var x : i64 = 1;
  var y : i64 = x;
  {
    var x : i64 = 2;
    x = x + 1;
    y = y + x;
  }
  x = x + y;
  return x;
}

var x : i64 = 5;
1 + ;
fn foo() : i64 {
  return x + 1;
}
{
  1 + ;
  var y : i64 = 6;
}
var z : i64 = x + y;
}
var w : i64 = 10;
if (w > 5) {
  w = w + 1;
}

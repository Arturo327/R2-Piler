fn main() : i64 {
  var a : i64 = 20;
  var b : i64 = 6;
  var s : i64 = a + b;
  var d : i64 = a - b;
  var m : i64 = a * b;
  var q : i64 = a / b;
  var r : i64 = a % b;
  var ban : i64 = a & b;
  var bor : i64 = a | b;
  var xo : i64 = a ^ b;
  var ls : i64 = a << b;
  var rs : i64 = a >> b;
  var ng : i64 = -a;
  var nt : i64 = ~a;
  return s + d + m + q + r + ban + bor + xo + ls + rs + ng + nt;
}

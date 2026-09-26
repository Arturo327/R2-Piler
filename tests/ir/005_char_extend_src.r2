fn chars(a : char, b : char) : char {
  var s : char = a + b;
  var d : char = a - b;
  var m : char = a * b;
  var q : char = a / b;
  var mo : char = a % b;
  var ban : char = a & b;
  var bor : char = a | b;
  var xo : char = a ^ b;
  var ls : char = a << b;
  var rs : char = a >> b;
  var ng : char = -a;
  var nt : char = ~a;
  var cl : i64 = a < b;
  var nl : i64 = !a;
  return s;
}
fn main() : i64 {
  var c : char = chars('a', 'b');
  return 0;
}

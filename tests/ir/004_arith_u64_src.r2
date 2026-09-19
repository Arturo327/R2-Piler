fn usum(a : u64, b : u64) : u64 {
  return a + b;
}
fn main() : i64 {
  var a : u64 = 20u;
  var b : u64 = 6u;
  var s : u64 = usum(a, b);
  var d : u64 = a - b;
  var m : u64 = a * b;
  var q : u64 = a / b;
  var r : u64 = a % b;
  var ban : u64 = a & b;
  var bor : u64 = a | b;
  var xo : u64 = a ^ b;
  var ls : u64 = a << b;
  var rs : u64 = a >> b;
  var c : i64 = a < b;
  return c;
}

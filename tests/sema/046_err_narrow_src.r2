fn take_u64(x : u64) : u64 {
  return x;
}
fn take_i64(x : i64) : i64 {
  return x;
}
fn main() : i64 {
  var a : i64 = 1;
  var b : u64 = 2u;
  var c : i64 = b;
  var d : u64 = a;
  var e : char = a;
  var f : u64 = take_u64(a);
  var g : i64 = take_i64(b);
  var h : u64 = 1;
  var i : char = 97;
  var j : char = 300;
  var k : i64 = 18446744073709551615u;
  return c;
}

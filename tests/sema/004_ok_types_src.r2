var d : i64 = 0x10;
var o : i64 = 0o17;
var b : i64 = 0b101;
var u : u64 = 0xFFu;
var big : u64 = 18446744073709551615u;
fn chars(a : char, c : char) : char {
  var r : char = a + c;
  var m : char = -a;
  var n : char = ~a;
  return r;
}
fn cmp(a : i64, e : i64) : i64 {
  var g : i64 = a > e;
  var l : i64 = a && e;
  var k : i64 = !a;
  return g + l + k;
}

var g : i64 = 42;
var h : u64 = g as u64;
fn id_u64(x : u64) : u64 {
  return x;
}
fn main() : i64 {
  var a : i64 = 1;
  var b : u64 = a as u64;
  var c : i64 = b as i64;
  var ch : char = 'a';
  var d : i64 = ch as i64;
  var e : char = a as char;
  var f : u64 = id_u64(b);
  var h2 : u64 = id_u64(a as u64);
  if (a as i64) {
    return d;
  }
  return 0;
}

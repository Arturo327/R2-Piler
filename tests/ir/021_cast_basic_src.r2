fn id_u64(x : u64) : u64 {
  return x;
}
fn main() : i64 {
  var a : i64 = 1;
  var b : i64 = a as i64;
  var c : u64 = a as u64;
  var d : i64 = c as i64;
  var ch : char = 'a';
  var e : i64 = ch as i64;
  var f : u64 = ch as u64;
  var g : u64 = id_u64(c);
  return e;
}

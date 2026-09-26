fn id_u64(x : u64) : u64 {
  return x;
}
fn main() : i64 {
  var a : i64 = 1;
  var b : u64 = 2u;
  var s : i64 = a << b;
  var t : i64 = a >> 2u;
  var c : char = 97;
  var c2 : char = a as char;
  var d : u64 = id_u64(b);
  return s;
}

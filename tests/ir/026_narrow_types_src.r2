fn id16(x : i16) : i16 {
  return x;
}
fn main() : i64 {
  var a : u8 = 200u;
  var b : i16 = 1000;
  var c : u32 = 3000000000u;
  var s : i16 = b + 1;
  var t : u8 = a + 1u;
  var n : u8 = ~a;
  var m : i16 = -b;
  var w : i16 = id16(b);
  var v : u64 = a as u64;
  return s as i64;
}

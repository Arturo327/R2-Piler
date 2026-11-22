pub fn main() : i64 {
  var a : i32 = 7;
  var b : i64 = a == 7;
  var c : u8 = a == 7;
  var d : i8 = !a;
  var e : u16 = a < 8 && a > 0;
  arr v : i64[2] = {1, 2};
  var p : ptr i64 = v;
  var q : ptr i64 = v + 1;
  var dist : u8 = q - p;
  return b + c + d + e + dist;
}

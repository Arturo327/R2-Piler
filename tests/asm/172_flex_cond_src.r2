pub fn main() : i64 {
  var a : i32 = 7;
  var b : i64 = a == 7;
  var c : u8 = a < 8;
  var d : i8 = !a;
  var e : u16 = a > 0 && a < 9;
  arr v : i64[2] = {1, 2};
  var p : ptr i64 = v;
  var q : ptr i64 = v + 1;
  var dist : u8 = q - p;
  return b * 100 + (c as i64) * 10 + (d as i64 + 1) * 4 + (e as i64) * 2 + (dist as i64);
}

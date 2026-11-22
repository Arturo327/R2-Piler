pub fn main() : i64 {
  var a : i32 = 7;
  var b : i64 = a == 7;
  var c : u8 = a < 8;
  var d : i32 = !a || b != 0;
  return b + c + d;
}

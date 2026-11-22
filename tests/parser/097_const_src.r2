const var g : i64 = 1;
const arr ga : i64[2] = {1, 2};

fn f(const p : i64) : const ptr i64 { return $g; }

pub fn main() : i64 {
  const var x : i8 = 3;
  static const var y : i64 = 4;
  const static var z : i64 = 5;
  return x + y + z;
}

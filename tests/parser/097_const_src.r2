var g : const i64 = 1;
arr ga : const i64[2] = {1, 2};

fn f(p : ptr const i64) : const ptr i64 { return $g; }

pub fn main() : i64 {
  var x : const i8 = 3;
  static var y : const i64 = 4;
  arr z : const (i64[2]) = {5, 6};
  var v : const i64[] = z;
  return (x as i64) + y + v[0];
}
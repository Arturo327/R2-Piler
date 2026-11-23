struct S { a : const i64; b : i64; }

var g : const i64 = 5;

fn add(x : const i64) : i64 { return x + 1; }

pub fn main() : i64 {
  arr a : const i64[2] = {1, 2};
  var s : S = {.a = 3, .b = 4};
  var cs : const S = s;
  var cp : ptr const i64 = $g;
  return add(@cp) + a[1] + cs.a + cs.b;
}

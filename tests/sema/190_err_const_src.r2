struct S { x : i64; y : const i64; }

fn bad(p : const i64) : void { p = 1; }

pub fn main() : i64 {
  var x : const i64 = 1;
  var noinit : const i64;
  arr a : const i64[2] = {1, 2};
  var n : i64 = 0;
  var p : ptr i64 = $n;
  var cp : ptr const i64 = $x;
  var q : const ptr i64 = p;
  var s : S = {.x = 1, .y = 2};
  var cs : const S = s;

  x = 2;
  x += 1;
  x++;
  a[0] = 3;
  q = p;
  p = cp;
  @cp = 4;
  cp[0] = 5;
  s.y = 6;
  cs.x = 7;
  var badany : const any;
  return x as i64;
}

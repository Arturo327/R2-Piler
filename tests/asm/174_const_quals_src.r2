type CI = const i32;

struct P { x : const i64; y : i64; }

fn addc(x : const i32) : const i32 { return x + 1; }

fn sum(s : P) : i64 { return s.x + s.y; }

fn pick(p : ptr const P) : i64 { return p.x + p.y; }

pub fn main() : i64 {
  var a : CI = 10;
  var b : i32 = addc(a);
  var p : P = {.x = 20, .y = 30};
  var cp : const P = p;
  var q : ptr const P = $p;
  var alias : i64 = cp.x + cp.y + (b as i64) + sum(cp);
  return alias + pick(q);
}

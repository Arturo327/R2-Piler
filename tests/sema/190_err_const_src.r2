fn bad(const p : i64) : void { p = 1; }

fn bad_ret() : const i64 { return 1; }

pub fn main() : i64 {
  const var x : i64 = 1;
  const arr a : i64[2] = {1, 2};
  const var noinit : i64;
  x = 2;
  a[0] = 3;
  x += 1;
  x++;
  bad(4);
  return x;
}

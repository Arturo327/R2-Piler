const var g : i64 = 40;
const arr a : i64[2] = {1, 2};

fn add1(const x : i64) : i64 { return x + 1; }

pub fn main() : i64 {
  const var x : i64 = g;
  var p : ptr i64 = $g;
  const var cp : ptr i64 = p;
  @cp = 2;
  return x + a[1] + add1(x) + @p;
}

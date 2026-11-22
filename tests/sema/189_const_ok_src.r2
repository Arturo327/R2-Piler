const var g : i64 = 7;
const arr ga : i64[3] = {1, 2, 3};

fn read(const p : i64) : i64 { return p + 1; }

fn ret() : const ptr i64 { return $g; }

pub fn main() : i64 {
  const var x : i64 = 3;
  const arr a : i64[3] = {4, 5, 6};
  var p : ptr i64 = $g;
  const var cp : ptr i64 = p;
  var c : i64 = 0;

  c = c + x;
  c = c + a[1];
  c = c + ga[2];
  c = c + read(x);
  c = c + @cp;
  @cp = 9;
  c = c + g;
  c = c + @ret();
  return c;
}

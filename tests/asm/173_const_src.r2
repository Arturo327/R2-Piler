struct S { a : const i64; b : i64; }

var g : const i64 = 40;
arr ga : const i64[2] = {1, 2};

fn add1(x : const i64) : i64 { return x + 1; }

fn peek(p : ptr const i64) : i64 { return @p; }

pub fn main() : i64 {
  var x : const i64 = g;
  var total : i64 = x + ga[1] + add1(x) + peek(ga);
  arr local : i64[3] = {7, 8, 9};
  var v : const i64[] = local;
  var s : S = {.a = 100, .b = 20};
  var cs : const S = s;
  var p : ptr const i64 = $g;
  total = total + v[2] + cs.a + cs.b + @p;
  return total;
}
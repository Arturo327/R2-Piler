fn cmp(a : u64, b : i64) : i64 {
  var x : i64 = a > b as u64;
  var y : i64 = b as u64 < a;
  var z : i64 = a == b as u64;
  return x + y + z;
}
fn main() : i64 {
  return cmp(5u, 1);
}

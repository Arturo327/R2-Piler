fn cmp(a : u64, b : i64) : i64 {
  var x : i64 = a > b as u64;
  var y : i64 = b as u64 < a;
  var z : i64 = a == b as u64;
  var w : i64 = a <= b as u64;
  return x + y + z + w;
}

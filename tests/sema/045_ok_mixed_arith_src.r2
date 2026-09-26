fn mixed(a : i64, b : u64, c : char) : i64 {
  var x : u64 = a as u64 + b;
  var y : i64 = a + c;
  var z : u64 = b + c as u64;
  var w : char = c + c;
  var s : i64 = a << b;
  var t : char = c >> a;
  var u : i64 = a - c;
  var v : u64 = b * c as u64;
  return s;
}

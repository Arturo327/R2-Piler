var y : u64 = x as u64;
var x : i64 = 300;
fn main() : i64 {
  var a : i64 = 300;
  var c : char = a as char;
  var u : u64 = 200u;
  var d : char = u as char;
  var e : char = c as char;
  var f : i64 = c as i64;
  if (a as i64) {
    return f;
  }
  return 0;
}

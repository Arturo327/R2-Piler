var a : i64 = 1 as i64 + 2;
var b : i64 = (1 + 2) as i64;
var c : i64 = -1 as i64;
var d : u64 = 1 as i64 as u64;
var e : u64 = f(1 as u64);
x = y as i64;
fn f(x : u64) : u64 {
  return x as u64;
}
var h : i64 = !256 as char as i64;

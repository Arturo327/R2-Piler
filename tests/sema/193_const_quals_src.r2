type CI = const i32;
type CP = ptr const i32;

struct Inner { v : const i32; }
struct Outer { in : Inner; items : const i32[2]; p : ptr const Inner; }

fn cb(a : const i32) : const i32 { return a; }

fn use(f : ptr fn(i32) : i32, p : CP, q : const ptr i64) : i64 {
  return (@p as i64) + @q + (f(1) as i64);
}

pub fn main() : i64 {
  arr msg : const i8[] = "hi";
  var a : CI = 1;
  var v : i32 = a;
  var o : Outer = {.in = {.v = 2}, .items = {3, 4}, .p = NULL};
  var oc : const Outer = o;
  var f : ptr fn(i32) : i32 = cb;
  var p : CP = $a;
  var q : const ptr i64 = NULL;
  var s : i64 = use(f, p, q);
  var eq : i32 = 0;
  if (o.p == oc.p) eq = 1;
  return (v as i64) + (oc.items[1] as i64) + s + (eq as i64) + msg.len;
}

struct Buf { p : ptr const void; q : ptr void; }

extern fn memcpy (dst : ptr void, src : ptr const void, n : u64) : ptr void;

var ro : ptr const void = NULL;

fn same (a : ptr const void, b : ptr const void) : i64 { return (a == b) as i64; }

fn to_ro (p : ptr i8) : ptr const void { return p; }

fn from_ro (p : ptr const void) : ptr const i8 { return p; }

fn use (s : ptr const i8, n : u64) : i64 {
  var p : ptr const void = s;
  var q : ptr void = p as ptr void;
  var r : ptr const i8 = p;
  var b : Buf = {.p = s, .q = q};
  memcpy(q, p, n);
  return (@r as i64) + same(p, b.p) + (@s as i64)
      + (sizeof(ptr const void) as i64) + (alignof(ptr const void) as i64);
}

pub fn main () : i64 {
  arr s : i8[2] = "x";
  ro = s;
  return use(ro, 1) + ((from_ro(ro) as ptr const void) == ro) as i64
      + (to_ro(s) == ro) as i64;
}
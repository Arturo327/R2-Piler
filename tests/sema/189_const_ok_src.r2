struct S { x : const i64; y : i64; }

var g : const i64 = 7;
arr ga : const i64[3] = {1, 2, 3};

fn read(p : const i64) : i64 { return p + 1; }

fn ret(p : ptr i64) : ptr const i64 { return p; }

fn field(s : S) : i64 { return s.x + s.y; }

pub fn main() : i64 {
  var x : const i64 = 3;
  arr a : const i64[3] = {4, 5, 6};
  var p : ptr const i64 = $g;
  var cp : ptr const i64 = p;
  var pi : ptr i64 = NULL;
  var q : const ptr i64 = pi;
  var s : S = {.x = 1, .y = 2};
  var cs : const S = s;
  var v : const i64[] = a;
  var sz : i64 = sizeof(const i64) + alignof(const i64);
  var c : i64 = 0;

  c = c + x;
  c = c + a[1];
  c = c + ga[2];
  c = c + read(x);
  c = c + @cp;
  c = c + @q;
  c = c + g;
  c = c + @ret(pi);
  c = c + field(s);
  c = c + cs.y;
  c = c + v[1];
  c = c + sz;
  return c;
}
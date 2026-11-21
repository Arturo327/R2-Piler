newtype P = ptr i64;
newtype Q = ptr i32;
newtype HP = ptr void;

struct S { p : P; x : i64; }

fn get (p : P) : i64 { return @p; }

pub fn main() : i64 {
    arr a : i64[4] = {11, 22, 33, 44};
    var raw : ptr i64 = a;
    var p : P = raw as P;
    var q : P = p + 2;
    var d : i64 = q - p;
    var v : i64 = q[0] + @q;
    var h : HP = NULL;
    if (h != NULL) { return 90; }
    p += 1;
    p++;
    if (p != q) { return 91; }
    var s : i64 = get(p);
    var st : S = {.p = p, .x = 5};
    arr b : i32[3] = {7, 8, 9};
    var br : ptr i32 = b;
    var qq : Q = br as Q;
    return d * 10 + v + s + st.x + qq[1];
}

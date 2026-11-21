pub newtype P = ptr i64;
pub newtype HP = ptr void;

struct S { p : P; x : i64; }

pub fn take (p : P) : i64 {
    var q : P = p + 1;
    var v : i64 = q[0] + @q;
    var d : i64 = q - p;
    q += 1;
    q++;
    if (q == NULL) { return -1; }
    if (q != p) { return v + d; }
    return 0;
}

pub fn main() : i64 {
    var s : P = NULL;
    var h : HP = NULL;
    var st : S = {.p = s, .x = 1};
    if (!h) { return st.x; }
    return take(s);
}

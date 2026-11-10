struct P { x : i64; y : i64; }
struct Q { p : P; z : i64; }

fn mk(v : i64) : P, i64 {
    var p : P;
    p.x = v;
    p.y = v + 1;
    return p, v * 2;
}

fn take(p : P) : i64 {
    return p.x + p.y;
}

fn main() : i64 {
    var a : P = mk(5);          // only the first value; buffer is bigger than P
    var q : Q;
    q.p = mk(7);                // struct assignment from a multi call
    var c : i64 = take(mk(9));  // by-value argument from a multi call
    return a.x + a.y + q.p.x + q.p.y + c;
}

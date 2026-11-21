newtype P = ptr i64;
newtype Q = ptr i64;
newtype V = ptr void;

pub fn f (p : P, raw : ptr i64) : i64 {
    var _a : P = raw;
    var _b : ptr i64 = p;
    var _c : Q = p as Q;
    var _d : P = 0;
    var _e : i64 = -p;
    var v : V = NULL;
    var _f : i64 = @v;
    return 0;
}

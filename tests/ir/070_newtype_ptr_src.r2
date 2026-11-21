newtype P = ptr i64;

pub fn main() : i64 {
    arr a : i64[3] = {1, 2, 3};
    var raw : ptr i64 = a;
    var p : P = raw as P;
    var q : P = p + 1;
    var d : i64 = q - p;
    var v : i64 = q[0] + @q;
    p += 1;
    p++;
    return d + v + @p;
}

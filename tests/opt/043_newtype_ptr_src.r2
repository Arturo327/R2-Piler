pub newtype P = ptr i64;

pub fn sum (p : P, n : i64) : i64 {
    var t : i64 = 0;
    var i : i64 = 0;
    while (i < n) {
        t += (p + i)[0];
        i += 1;
    }
    return t;
}

pub fn main() : i64 {
    arr a : i64[4] = {3, 5, 7, 9};
    var raw : ptr i64 = a;
    return sum(raw as P, 4);
}

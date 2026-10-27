fn main() : i64 {
    var a : i64 = 1;
    var b : i64 = 2;
    (a + b) += 3;
    a += b += 1;
    a <<= b;
    return a;
}

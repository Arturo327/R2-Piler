fn apply_mask(a: i64, b: i64) : i64 {
    var mask: i64 = 255;
    var x: i64 = (a & mask) | (b ^ mask);
    x = x << 2;
    x = x >> 1;
    x = ~x;
    x = a + b - a * b / 2;
    var ok: i64 = 0;
    if (x > 0 && x < 1000 || !(x == 0)) {
        ok = 1;
    }
    if (x >= 10 && x <= 20 || x != 30) {
        ok = ok + 1;
    }
    return ok;
}

fn main() {
    var r: i64 = apply_mask(12, 34);
    var s: i64 = r ^ 170 & 85 | r;
}

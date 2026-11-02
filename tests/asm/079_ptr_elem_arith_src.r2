var base : ptr i64 = $anchor;
var anchor : i64 = 42;

fn diff (a : ptr i64, b : ptr i64) : i64 {
    return a - b;
}

fn bump (p : ptr i64) : ptr i64 {
    p += 2;
    p++;
    return p;
}

fn adv (p : ptr i64, n : i8) : ptr i64 {
    return p + n;
}

fn by (p : ptr i64, n : i16) : ptr i64 {
    p += n;
    return p;
}

fn main () : i64 {
    var b : ptr i64 = base;
    var bytes : ptr i8 = b as ptr i8;
    var eight : ptr i64 = (bytes + 8) as ptr i64;
    var up : i64 = ((b + 1) as ptr i8) - (b as ptr i8);
    var neg : i64 = diff(b, eight);
    var many : i64 = (bump(b) as ptr i8) - (b as ptr i8);
    var many_elem : i64 = diff(bump(b), b);
    var nadv : i64 = (adv(b, 2 as i8) as ptr i8) - (b as ptr i8);
    var nby : i64 = (by(b, 3 as i16) as ptr i8) - (b as ptr i8);

    return (diff(eight, b) == 1) + (up == 8) * 2 + (neg == -1) * 4
            + (many == 24) * 8 + (many_elem == 3) * 16
            + (nadv == 16) * 32 + (nby == 24) * 64 + (@base == 42) * 128;
}
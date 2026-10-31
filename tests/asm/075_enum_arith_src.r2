enum Flags : u8 {
    None = 0,
    A = 1,
    B = 2,
    AB = 3,
    Alias = 3,
}

enum Small : i8 {
    Min = -2,
    Zero = 0,
    Two = 2,
}

fn main () : i64 {
    var f : Flags = Flags.A;
    var g : Flags = Flags.B;
    var s : Small = Small.Min;
    var q : Small = Small.Two;
    var k : i64 = 2;
    var r : i64 = 0;

    f = f | g;
    g = f & Flags.B;

    if (f == Flags.AB) {
        r = r + 10;
    }
    if (g == Flags.B) {
        r = r + 20;
    }

    r = r + (s + Small.Two) as i64;
    r = r + (Flags.AB << k) as i64;
    r = r + (Flags.Alias ^ Flags.A) as i64;
    r = r + (s / q) as i64;
    r = r + (s * q) as i64;

    return r;
}

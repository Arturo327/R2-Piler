newtype Seconds = u32;
newtype Tiny = i8;

fn bump (s : Seconds) : Seconds {
    return s + 1;
}

fn main () : i64 {
    var s : Seconds = 7;
    var t : Seconds = bump(s);
    var n : Tiny = -2;
    n = n * 3;
    return (t as i64) + (n as i64);
}

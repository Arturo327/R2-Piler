fn step (p : ptr i64, n : i64) : ptr i64 {
    return p + n;
}

fn back (p : ptr i32, n : i32) : ptr i32 {
    return p - n;
}

fn gap (a : ptr i64, b : ptr i64) : i64 {
    return a - b;
}

fn narrow (c : i8, p : ptr i8) : ptr i8 {
    return p + c;
}

fn bump (p : ptr i64, n : i64) : ptr i64 {
    p += n;
    p++;
    return p;
}

fn main () : i64 {
    return 0;
}

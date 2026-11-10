fn two(a : i64) : i64, u32 {
    return a;
}

fn one(a : i64) : i64 {
    return a, a;
}

fn bad_void() : void, i64 {
    return 1, 2;
}

extern fn ext() : i64[];

fn main() : i64, i64 {
    return 1, 2;
}

fn entry() : i64 {
    var a : i64;
    var b : u32;
    var c : i64;
    arr v : i64[2];

    a, b, c = two(1);
    a, b = one(1);
    var d : i64, e : i64 = 3;
    1, a = two(1);
    v, a = two(1);
    return a;
}
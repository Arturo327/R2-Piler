arr g : i64[3] = {1, 2, 3};

fn split(a : i64, b : i64) : i64, u32 {
    return a, b as u32;
}

fn pick(x : i64[]) : i64[] {
    return x;
}

fn row(m : i32[2][3][]) : i32[2][3][] {
    return m;
}

fn shape() : i64[3] {
    return g;
}

fn main() : i64 {
    var a : i64;
    var b : u32;
    a, b = split(1, 2);
    var c : i64, d : u32 = split(3, 4);
    var e : i64 = split(5, 6);
    return a;
}
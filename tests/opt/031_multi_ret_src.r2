arr g : i64[4] = {1, 2, 3, 4};

fn split(a : i64) : i64, u32 {
    return a / 2, (a * 3) as u32;
}

fn pick(x : i64[]) : i64[] {
    return x;
}

fn main() : i64 {
    var a : i64;
    var b : u32;
    a, b = split(9);
    var s : i64[] = pick(g);
    return a + (b as i64) + s[1];
}

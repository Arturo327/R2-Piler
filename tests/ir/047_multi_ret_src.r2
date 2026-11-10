fn split(a : i64) : i64, u32 {
    return a / 2, (a * 3) as u32;
}

fn main() : i64 {
    var a : i64;
    var b : u32;
    a, b = split(9);
    var c : i64, d : u32 = split(7);
    var e : i64 = split(5);
    return a + (b as i64) + c + (d as i64) + e;
}
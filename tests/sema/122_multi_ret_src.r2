arr g : i64[3] = {1, 2, 3};
arr gl : i32[2][3] = {{1, 2, 3}, {4, 5, 6}};

fn split(a : i64) : i64, u32 {
    return a, a as u32;
}

fn pick(x : i64[]) : i64[] {
    return x;
}

fn row(m : i32[2][3][]) : i32[2][3][] {
    return m;
}

fn shape(x : i64[3]) : i64[3] {
    return x;
}

fn take(m : i32[2][3][]) : i64 {
    return m.len;
}

fn relay(m : i32[2][3][]) : i64 {
    return take(row(m));
}

fn main() : i64 {
    var a : i64;
    var b : u32;
    a, b = split(9);
    var c : i64, d : u32 = split(7);
    var e : i64 = split(5);
    var s : i64[] = pick(g);
    var n : i64 = shape(g)[0] + pick(g).len + pick(g)[1];
    return a + (b as i64) + c + (d as i64) + e + s.len + n;
}
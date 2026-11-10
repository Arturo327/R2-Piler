struct S { a : i64; b : u32; }
struct P { x : i64; y : i64; }

arr g : i64[4] = {1, 2, 3, 4};

fn two(v : i64) : i64, u32 {
    return v + 1, (v + 2) as u32;
}

fn q(v : i64) : P, i64[] {
    var p : P;
    p.x = v;
    p.y = v * 2;
    return p, g;
}

fn main() : i64 {
    var s : S;
    arr v : i64[4];
    s.a, s.b = two(10);
    v[0], v[1] = two(20);
    g[2], g[3] = two(30);
    var pp : ptr i64 = v;
    @pp, v[1] = two(40);

    var a : P;
    var sl : i64[];
    a, sl = q(5);
    return s.a + (s.b as i64) + v[1] + g[3] + a.x + a.y + sl[3];
}
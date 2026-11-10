arr g : i64[4] = {10, 20, 30, 40};
arr m : i64[3][2] = {{1, 2}, {3, 4}, {5, 6}};

fn pick(x : i64[]) : i64[] {
    return x;
}

fn whole() : i64[] {
    return g;
}

fn level() : i64[4] {
    return g;
}

fn main() : i64 {
    var s : i64[] = pick(g);
    var t : i64[] = pick(m[1]);
    var n : i64 = pick(g).len;
    var e : i64 = pick(g)[1];
    var u : i64[] = whole();
    var s2 : i64[] = level();
    var v : i64 = level()[2];
    return s[0] + t.len + n + e + u.len + s2[1] + v;
}
arr g : i64[3] = {7, 8, 9};
arr m : i64[2][3] = {{1, 2, 3}, {4, 5, 6}};

fn shape() : i64[3] {
    return g;
}

fn row(i : i64) : i64[3] {
    return m[i];
}

fn len_of(x : i64[3]) : i64 {
    return x[0] + x[2];
}

fn main() : i64 {
    var s : i64[] = shape();
    var t : i64[] = row(1);
    var u : ptr i64 = row(0);
    return s[1] + t.len + u[2] + len_of(shape()) + s.len;
}

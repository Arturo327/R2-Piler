arr g : i64[3] = {1, 2, 3};
arr m : i64[2][3] = {{1, 2, 3}, {4, 5, 6}};

fn shape() : i64[3] {
    return g;
}

fn row(i : i64) : i64[3] {
    return m[i];
}

fn main() : i64 {
    return shape()[0] + row(1)[2];
}

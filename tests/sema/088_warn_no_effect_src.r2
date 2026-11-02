fn side(x : i64) : i64 {
    return x + 1;
}

fn main() : i64 {
    var x : i64 = 1;
    x + 1;
    side(x);
    x = x + 2;
    x++;
    return x;
}
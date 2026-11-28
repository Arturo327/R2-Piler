fn f(x : i64[], v : i64[]) : i64 { x = v; return x.len; }
pub fn main() : i64 {
    arr a : i64[2] = {7, 8};
    arr b : i64[3] = {1, 2, 3};
    return f(a, b);
}

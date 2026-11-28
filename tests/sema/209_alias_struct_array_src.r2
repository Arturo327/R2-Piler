struct S { x : i64; }
type SA = S[2];

pub fn main() : i64 {
    arr a : SA = {{.x = 7}, {.x = 0}};
    return a[0].x;
}

fn main() : i64 {
    var x : i64 = 0;
    var p : ptr i64 = $x;

    if (x == 1) { p = NULL; } else { p = NULL; }
    var v : i64 = @p;
    return v;
}
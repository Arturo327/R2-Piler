fn main() : i64 {
    var x : i64 = 5;
    var p : ptr i64 = NULL;
    var q : ptr i64 = $x;
    var a : i64 = @p;
    var b : i64 = @q;
    return a + b;
}
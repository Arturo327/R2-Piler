var g : i64 = 5;
var s : ptr i8 = "abc";
var p : ptr i64 = $g;
var n : ptr i64 = NULL;

fn main () : i64 {
    @p = @p + 1;
    var r : i64 = 0;
    if (n == NULL) { r = 1; }
    return g + (@s as i64) - 97 + r;
}

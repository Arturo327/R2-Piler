var g : i64 = 5;
var s : ptr i8 = "abc";
var p : ptr i64 = $g;
var v : ptr void = $g as ptr void;
var n : ptr i64 = NULL;

fn main () : i64 {
    @p = @p + 1;
    return g + (@s as i64) + (n == NULL) as i64;
}

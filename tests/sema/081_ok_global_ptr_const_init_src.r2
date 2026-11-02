var g : i64 = 5;
var s : ptr i8 = "hello";
var p : ptr i64 = $g;
var v : ptr void = $g as ptr void;
var n : ptr i64 = NULL;
var n2 : ptr i32 = NULL as ptr i32;

fn main () : i64 {
    var r : i64 = @s as i64;
    @p = @p + 1;
    if (n == NULL) { r = r + 1; }
    if (v != NULL) { r = r + 2; }
    if (n2 == NULL) { r = r + 4; }
    return r + g;
}

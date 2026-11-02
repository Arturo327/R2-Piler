var g : i64 = 7;
var s : ptr i8 = "hi";
var p : ptr i64 = $g;
var v : ptr void = $g as ptr void;
var n : ptr i64 = NULL;

fn main () : i64 {
    @p = @p + 1;
    var c : i64 = @s as i64;
    var z : i64 = 0;
    if (n == NULL) { z = z + 1; }
    if (v != NULL) { z = z + 2; }
    return g + c + z;
}

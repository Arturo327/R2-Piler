var g : i64 = 7;
var gp : ptr i64 = NULL;

fn deref (p : ptr i64) : i64 {
    return @p;
}

fn main () : i64 {
    var x : i64 = 3;
    var p : ptr i64 = $x;
    @p = @p + 1;
    @p++;
    var q : ptr i64 = $@p;
    var n : i64 = @q;
    var s : ptr i8 = "hello";
    var t : ptr i8 = "hello";
    var u : ptr void = p as ptr void;
    if (u == NULL) { n = n + 100; }
    gp = $g;
    return deref(p) + n;
}

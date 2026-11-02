var g : i64 = 10;
var gp : ptr i64 = NULL;

fn swap(p : ptr i64, q : ptr i64) {
    var t : i64 = @p;
    @p = @q;
    @q = t;
}

fn main() : i64 {
    var x : i64 = 3;
    var y : i64 = 4;
    var p : ptr i64 = $x;
    var q : ptr i64 = $y;

    var nx : i64 = @p;
    @p = @p + 1;
    @p++;
    var pp : ptr i64 = $@p;
    var ppp : ptr ptr i64 = $p;
    var qq : ptr i64 = @ppp;

    var nul : ptr i64 = NULL;
    var is_null : i64 = 0;
    if (nul == NULL) { is_null = 1; }
    if (NULL == NULL) { is_null = is_null + 2; }

    var s : ptr i8 = "hello";
    var t : ptr i8 = "hello";
    var same : i64 = 0;
    if (s == t) { same = 1; }
    if (s != NULL) { same = same + 2; }

    gp = $g;
    @gp = @gp + 5;

    swap(p, q);

    return nx + x + y + is_null + same + g;
}

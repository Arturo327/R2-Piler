type P = ptr i64;

fn bump(p : ptr i64) : ptr i64 {
    @p = @p + 1;
    return p;
}

fn main() : i64 {
    var x : i32 = 42;
    var p : ptr i32 = $x;
    var q : ptr ptr i64;
    var r : ptr ptr ptr i64;
    var n : ptr void = NULL;
    var s : i64 = sizeof(i64) + sizeof(ptr i32);
    var u : i64 = sizeof(P);
    var v : i64 = sizeof(@p);
    var z : i64 = @p as i64;
    var c : ptr i64 = p as ptr i64;
    var d : ptr i64 = bump(c);
    @p = 7;
    p = p + 1;
    p++;
    @p++;
    return 0;
}

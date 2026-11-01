type P = ptr i64;
type Q = ptr P;

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
    var s : u64 = sizeof(i64) + sizeof(ptr i32);
    var u : u64 = sizeof(P);
    var v : u64 = sizeof(@p);
    var c : ptr i64 = p as ptr i64;
    var d : ptr i64 = bump(c);
    var e : ptr i32 = p + 1;
    var f : i64 = p - e;
    var g : i64 = p == NULL;
    var h : i64 = NULL == NULL;
    var i : ptr i8 = "hi";
    var j : u64 = sizeof("abc");
    @p = 7;
    p = p + 1;
    p++;
    @p++;
    p += 1;
    p -= 1;
    return 0;
}

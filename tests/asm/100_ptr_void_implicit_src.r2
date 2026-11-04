extern fn malloc (size : u64) : ptr void;
extern fn free (p : ptr void) : void;
extern fn memcpy (dst : ptr void, src : ptr void, n : u64) : ptr void;

fn store(p : ptr i32, v : i32) : i32 {
    @p = v;
    return @p;
}

fn main() : i64 {
    var p : ptr i32 = malloc(4);
    var q : ptr i32 = malloc(4);
    var v : ptr void = p;
    @p = 30;
    @q = 12;
    memcpy(q, v, 4);
    var r : i32 = store(v, 30);
    var s : i32 = @q;
    arr a : i32[2] = { 1, 2 };
    arr b : i32[2] = { 0, 0 };
    memcpy(b, a, 8);
    var t : i32 = b[1];
    free(p);
    free(q);
    return (s + r + t) as i64;
}

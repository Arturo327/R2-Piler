extern fn malloc (size : u64) : ptr void;
extern fn free (p : ptr i8) : void;

fn store(p : ptr i32, v : i32) : i32 {
    @p = v;
    return @p;
}

fn make() : ptr i8 {
    return malloc(2);
}

fn main() : i64 {
    var p : ptr i32 = malloc(4);
    var q : ptr i8 = make();
    var total : i32 = store(p, 30);
    @q = 12;
    var r : i64 = total as i64 + @q as i64;
    free(p as ptr i8);
    free(q);
    return r;
}

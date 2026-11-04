extern fn malloc (size : u64) : ptr void;
extern fn free (p : ptr void) : void;
extern fn memset (dst : ptr void, c : i32, n : u64) : ptr void;

fn take(p : ptr i32) : i32 {
    return @p;
}

fn make() : ptr i16 {
    return malloc(2);
}

fn wipe(s : i32[], n : u64) : i32 {
    memset(s, 0, n);
    return s.len as i32;
}

fn main() : i64 {
    var a : ptr i32 = malloc(4);
    var b : ptr u8 = malloc(1);
    var c : ptr i16 = make();
    var d : ptr ptr i64 = malloc(8);
    var v : ptr void = a;
    var w : ptr void = c;
    arr e : i32[2] = { 1, 2 };
    @a = 1;
    @b = 2;
    @c = 3;
    @d = NULL;
    memset(e, 0, 8);
    free(a);
    free(v);
    free(w);
    return (take(v) + @b + wipe(e, 8)) as i64 + @c as i64 + (@d == NULL);
}

extern fn malloc (size : u64) : ptr void;

fn take(p : ptr i32) : i32 {
    return @p;
}

fn make() : ptr i16 {
    return malloc(2);
}

fn main() : i64 {
    var a : ptr i32 = malloc(4);
    var b : ptr u8 = malloc(1);
    var c : ptr i16 = make();
    var d : ptr ptr i64 = malloc(8);
    @a = 1;
    @b = 2;
    @c = 3;
    @d = NULL;
    return (take(a) + @b) as i64 + @c as i64 + (@d == NULL);
}

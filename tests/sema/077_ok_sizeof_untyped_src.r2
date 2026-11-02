type P = ptr i64;

fn main() : i64 {
    var a : i64 = sizeof(i64);
    var b : u64 = sizeof(i32);
    var c : i8 = sizeof(i8);
    var d : u32 = sizeof(ptr i64);
    var e : u64 = sizeof(P);
    var f : u64 = sizeof("abc");
    var g : i64 = 1 + sizeof(i8);
    var h : u16 = sizeof(u16) * 2;
    var p : ptr i64 = NULL;
    var q : ptr i64 = p + sizeof(i64);
    var r : ptr i64 = sizeof(i64) + p;
    var s : i64 = q - p;
    var t : i64 = sizeof(i64) - sizeof(i32);
    var u : i64 = sizeof(i32);
    var v : i8 = 1 + sizeof(i8);
    var w : i64 = sizeof(P) as i64;
    return a + (b as i64) + (c as i64) + (d as i64) + (e as i64) + (f as i64) + g + (h as i64) + s + t + u + (v as i64) + w;
}

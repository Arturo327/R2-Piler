fn main() : i64 {
    var a : i64 = 1;
    var same : i64 = a as i64;
    var wide : u64 = a as u64;
    var ch : char = 'a';
    var num : i64 = ch as i64;
    return same + (wide as i64) + num;
}
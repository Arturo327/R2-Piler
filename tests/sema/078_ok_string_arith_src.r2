fn main() : i64 {
    var p : ptr i8 = "abc" + 1;
    var q : ptr i8 = 1 + "abc";
    var r : i64 = p - "abc";
    var d : i64 = "abc" - "ab";
    var e : i64 = p == "abc";
    var f : i64 = "abc" == "abcd";
    var g : i64 = p != "abc";
    return r + d + e + f + g;
}

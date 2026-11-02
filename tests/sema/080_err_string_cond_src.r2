fn main() : i64 {
    if ("abc") return 1;
    if (!"abc") return 2;
    if ("abc" && 1) return 3;
    if (1 || "abc") return 4;
    return -"abc";
}

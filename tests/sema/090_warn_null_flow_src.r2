fn main() : i64 {
    var x : i64 = 0;
    var p : ptr i64 = $x;
    var n : ptr i64 = NULL;
    var sum : i64 = 0;

    if (x == 1) { p = NULL; } else { p = $x; }
    if (x == 2) { p = NULL; }
    while (x == 3) { p = NULL; }
    var ok : i64 = (x == 4) && (p = NULL) != NULL;

    sum = sum + ok;
    if (@p == 0) { sum = sum + 1; }
    if (@n == 0) { sum = sum + 2; }
    return sum;
}
fn max(a: i64, b: i64) : i64 {
    if (a >= b) {
        return a;
    } elif (a == b) {
        return a;
    } else {
        return b;
    }
}

fn factorial(n: i64) : i64 {
    var result: i64 = 1;
    var i: i64 = 2;
    while (i <= n) {
        result = result * i;
        i = i + 1;
    }
    return result;
}

fn main() {
    var x: i64 = 10;
    var y: i64 = 20;
    var m: i64 = max(x, y);
    var f: i64 = factorial(5);
    if (m != 20 || f == 0) {
        return;
    }
    var done: i64 = 1;
}

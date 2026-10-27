var g : i64 = 1;
var h : i64 = (g += 2);

fn main() : i64 {
    var a : i8 = 1;
    var b : i8 = 2;
    a += b;
    a++;
    a--;
    var u : u64 = 10;
    u >>= 1;
    var x : i64 = 0;
    x = x + 1;
    x += 1;
    x++;
    return (a as i64) + (x as i64) + (u as i64) + h;
}

var g : i64 = 1;
var h : i64 = (g += 2);

fn main() : i64 {
    var x : i64 = 10;
    x += 3;
    x -= 1;
    x *= 2;
    x /= 4;
    x %= 5;
    x &= 7;
    x |= 8;
    x ^= 1;
    x <<= 1;
    x >>= 1;
    x++;
    x--;
    var a : u8 = 250;
    a += 10;
    a++;
    var i : i64 = 0;
    var s : i64 = 0;
    for (; i < 3; i++) {
        s += i;
    }
    return x + s + g + h + (a as i64);
}

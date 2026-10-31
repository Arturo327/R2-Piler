newtype Small = i8;
newtype Byte = u8;

fn mix (a : Small, b : Byte) : i64 {
    var s : Small = a;
    var u : Byte = b;
    var three : Small = 3;
    var i : i64 = 0;
    while (i < 1000) {
        s = s + 1;
        u = u + 3;
        i++;
    }
    var q : Small = s / three;
    var m : Small = s % 5;
    var r : Byte = u % 7;
    if (q < m) {
        q = q + 1;
    }
    return (q as i64) + (m as i64) + (r as i64);
}

fn main () : i64 {
    var s : Small = 50;
    var u : Byte = 250;
    var x : i64 = mix(s, u);
    return x % 256;
}

newtype Seconds = u32;
newtype Small = i8;

enum E : u8 { A, B }
newtype FromEnum = E;

var clock : Seconds = 10;

fn add (a : Seconds, b : Seconds) : Seconds {
    return a + b;
}

fn main () : i64 {
    var s : Seconds = 7;
    var e : FromEnum = E.B as FromEnum;
    var small : Small = -2;
    s = add(s, s);
    s += 3;
    s++;
    small = small * 2;
    small = small as Small;
    return (s as i64) + (e as i64) + (small as i64) + (clock as i64);
}

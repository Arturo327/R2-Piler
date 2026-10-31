newtype Seconds = u32;
newtype Byte = u8;

var clock : Seconds = 10;

fn main () : i64 {
    var s : Seconds = 7;
    var b : Byte = 200;
    s = s * 8;
    s = s + 0;
    b = b / 4;
    b = b % 8;
    return (s as i64) + (b as i64) + (clock as i64);
}

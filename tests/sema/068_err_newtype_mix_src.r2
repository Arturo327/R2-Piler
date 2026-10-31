newtype Seconds = u32;
newtype Millis = u32;

fn take (u : u32) : u32 { return u; }
fn give () : Seconds { return 1; }

fn main () : i64 {
    var s : Seconds = 3;
    var m : Millis = 4;
    var u : u32 = 5;
    u = s;
    s = u;
    take(s);
    m = s as Millis;
    var bad : Seconds = 4294967296;
    s = give();
    return (s as i64) + (u as i64);
}

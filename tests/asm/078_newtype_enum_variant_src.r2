enum Color : u8 { Red = 3, Green = 9, Blue = 20 }
newtype Paint = Color;
type Tint = Color;
newtype Shade = Paint;

fn value (p : Paint) : i64 { return p as i64; }

fn main () : i64 {
    var p : Paint = Paint.Green;
    var t : Tint = Tint.Red;
    var s : Shade = Shade.Blue;
    var sum : i64 = value(p) + (t as i64) + (s as i64);
    if (p == Paint.Green) {
        sum = sum + 1;
    }
    return sum;
}

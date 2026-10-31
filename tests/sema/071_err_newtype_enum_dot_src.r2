newtype Plain = u32;
newtype Stamp = Dias;
enum Dias : u8 { Lunes, Martes }

fn main () : i64 {
    var p : Plain = Plain.Lunes;
    var e : Dias = Stamp.Lunes;
    var q : Stamp = Stamp.Nope;
    return (p as i64) + (e as i64) + (q as i64);
}

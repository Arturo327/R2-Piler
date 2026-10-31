enum Dias : u8 { Lunes = 1, Martes, Miercoles }
newtype Stamp = Dias;
type Day = Dias;
newtype Deep = Stamp;
type DeepAlias = Deep;

var first : Stamp = Stamp.Lunes;

fn pick (s : Stamp) : i64 { return s as i64; }

fn main () : i64 {
    var s : Stamp = Stamp.Martes;
    var d : Day = Day.Lunes;
    var x : Deep = Deep.Miercoles;
    var y : DeepAlias = DeepAlias.Lunes;
    return pick(s) + (d as i64) + (x as i64) + (y as i64) + (first as i64);
}

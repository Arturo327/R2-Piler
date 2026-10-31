type Index = u32;
type Alias = Index;

enum Dias : u8 { Lunes, Martes }
type Day = Dias;

var g : Index = 1;

fn f (i : Alias) : Index {
    return i + 1;
}

fn main () : i64 {
    var x : Index = 2;
    var d : Day = Day.Martes;
    var e : Dias = Dias.Lunes;
    return (x as i64) + (d as i64) + (e as i64) + (g as i64);
}

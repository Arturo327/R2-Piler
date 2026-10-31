enum Dias : u8 {
    Lunes = 0,
    Martes,
    Miercoles,
}

var today : Dias = Dias.Martes;
var count : i64 = 3;

fn main () : i64 {
    return today as i64;
}

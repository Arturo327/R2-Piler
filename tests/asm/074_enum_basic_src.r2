enum Dias : u8 {
    Lunes = 0,
    Martes,
    Miercoles,
}

fn main () : i64 {
    var d : Dias = Dias.Martes;
    var r : i64 = 0;

    if (d == Dias.Martes) {
        r = r + 10;
    }
    if (d == Dias.Lunes) {
        r = r + 100;
    }
    if (d < Dias.Miercoles) {
        r = r + 5;
    }

    return r + (Dias.Miercoles as i64);
}

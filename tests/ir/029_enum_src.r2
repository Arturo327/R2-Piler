enum Dias : u8 {
    Lunes = 0,
    Martes,
    Miercoles,
}

enum Signed : i8 {
    Neg = -1,
    Zero,
}

var today : Dias = Dias.Martes;

fn next (d : Dias) : Dias {
    return d + 1;
}

fn main () : i64 {
    var d : Dias = Dias.Lunes;
    var s : Signed = Signed.Neg;
    var u : u8 = d as u8;
    var back : Dias = u as Dias;

    return next(d) as i64 + (s as i64) + (back as i64);
}

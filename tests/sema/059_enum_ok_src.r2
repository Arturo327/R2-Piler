enum Dias : u8 {
    Lunes = 0,
    Martes,
    Miercoles,
}

enum Signed : i8 {
    Neg = -1,
    Zero,
    One,
}

enum Flags {
    None = 0,
    A = 1,
    B = 2,
    AB = 3,
    Alias = 1,
}

enum Wide : u64 {
    Max = ~0u,
}

enum Casts : u8 {
    A = (300 as u8),
    B,
    C = (-1 as u8),
}

var today : Dias = Dias.Martes;

fn next (d : Dias) : Dias {
    return d + 1;
}

fn first (f : Flags) : Flags {
    return f;
}

fn main () : i64 {
    var d : Dias = Dias.Lunes;
    var lit : Dias = 1;
    var s : Signed = Signed.Neg;
    var w : Wide = Wide.Max;
    var c : Casts = Casts.B;
    var u : u8 = d as u8;
    var back : Dias = u as Dias;
    var sum : i64 = (Dias.Lunes + Dias.Martes) as i64;
    var cmp : i64 = 0;

    if (d == Dias.Lunes) {
        cmp = 1;
    }
    if (s < Signed.Zero) {
        cmp = cmp + 1;
    }
    if (lit == back) {
        cmp = cmp + 1;
    }

    return next(d) as i64 + sum + cmp + (first(Flags.A) as i64)
            + (w as i64) - (c as i64);
}

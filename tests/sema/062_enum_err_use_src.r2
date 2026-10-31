enum Dias : u8 {
    Lunes,
    Martes,
}

enum Other : u8 {
    A,
}

var v : i64 = 1;

fn take (d : Dias) : i64 {
    return d as i64;
}

fn main () : i64 {
    var a : Dias = Dias.Nope;
    var b : Dias = v.Dias;
    var c : Dias = (1 + 2).Lunes;
    var e : Dias = Dias;
    var g : i64 = Dias;
    var h : u8 = a;
    var i : Dias = 1u as u8;
    var j : Other = a as Other;
    var k : i64 = take(v);
    var l : Dias = 300;
    var m : Nope = 1;
    var n : i64 = Dias();

    Dias = 1;
    Dias.Lunes = 1;
    return 0;
}

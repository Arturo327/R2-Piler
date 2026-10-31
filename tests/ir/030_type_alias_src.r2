type Index = u32;

fn bump (i : Index) : Index {
    return i + 1;
}

fn main () : i64 {
    var x : Index = 3;
    var y : Index = bump(x);
    return (y as i64);
}

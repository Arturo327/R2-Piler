class C {
    x : i64;
    init (self, v : i64) { self.x = v; }
    destroy () { }
}

pub fn main() : i64 {
    var c : C = C(7);
    drop c;
    reset c(9);
    return c.x;
}

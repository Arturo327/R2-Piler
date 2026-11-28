class C {
    x : i64;
    fn set(self, v : i64) : void { self.x = v; }
}
pub fn main() : i64 {
    var c : const C = C();
    c.set(5);
    return c.x;
}

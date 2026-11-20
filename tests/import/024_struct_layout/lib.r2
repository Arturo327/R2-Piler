pub packed struct H { a : u8; b : u16; }
pub union U { x : i64; y : i32; }
pub struct B { a : u8 : 3; b : u5; }

pub fn sum (h : H) : i64 { return h.a + h.b; }

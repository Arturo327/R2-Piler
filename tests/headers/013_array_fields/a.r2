export struct S { a : u8[3]; b : i32[2]; };
export fn sum (s : ptr S) : i64 { return s.a[0] + s.a[1] + s.a[2] + s.b[0] + s.b[1]; }

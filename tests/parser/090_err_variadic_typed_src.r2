// A typed variadic parameter needs a name and must be the last parameter.
fn not_last (x : i64, rest : ...i64, y : i64) : i64 { return x; }
fn unnamed (x : i64, ...i64) : i64 { return x; }

pub fn main () : i64 { return 0; }
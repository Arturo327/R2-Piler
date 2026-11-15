// A typed variadic parameter is written 'name : ...T' and must be the last
// one. The parser keeps the element type on the parameter and flags the list;
// sema later turns it into a slice parameter.
fn sum (rest : ...i64) : i64 { return rest.len; }

fn show (tag : ptr i8, count : i32, rest : ...ptr i8) : i64 { return count as i64; }

extern fn printf (fmt : ptr i8, ...) : i32;

pub fn main () : i64 { return sum(1, 2); }
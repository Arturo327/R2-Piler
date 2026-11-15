// '...' is a single token (TOK_ELLIPSIS).
extern fn printf (fmt : ptr i8, ...) : i32;
extern fn trace (...);
fn plain (a : i64, b : i64) : i64 { return a + b; }

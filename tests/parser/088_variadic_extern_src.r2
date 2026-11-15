// A variadic parameter list is flagged on NODE_ARGS_DEC.
extern fn printf (fmt : ptr i8, ...) : i32;
extern fn trace (...);
extern fn memset (s : ptr void, c : i32, n : u64) : ptr void;

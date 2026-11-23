pub type CI = const i32;

pub struct Box { v : const i64; }

pub struct W { v : CI; }

pub var g : const i64 = 11;

pub fn make (x : i64) : Box { return {.v = x} as Box; }

pub fn read (b : Box) : i64 { return b.v; }

pub fn peek (p : ptr const i64) : i64 { return @p; }

pub fn addr () : ptr const i64 { return $g; }

pub fn take (x : CI) : i64 { return x as i64; }

pub fn wrap () : W { return {.v = 5} as W; }

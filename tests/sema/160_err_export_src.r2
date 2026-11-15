struct P { x : i64; }

export fn s (p : P) : i64 { return p.x; }
export fn sl (a : i64[]) : i64 { return a[0]; }
export fn multi () : i64, i64 { return 1, 2; }
export fn rets () : P { return {.x = 1} as P; }
export fn main () : i64 { return 0; }

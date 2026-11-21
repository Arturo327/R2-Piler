fn seed (x : i64) : i64 { return x + 1; }

pub var g : i64 = seed(41);

pub fn value () : i64 { return g; }

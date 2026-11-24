import "lib";

pub fn make (v : i64) : lib::Count { return v; }

pub fn boxed (x : lib::Box) : i64 { return x.v; }

pub fn red () : lib::Color { return lib::Color.Red; }

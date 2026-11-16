module "m$x";

fn f (a : any) : any { return a; }
fn g (rest : ...any) : i64 { return rest.len; }
fn h (s : any[], n : i64) : i64 { return g(s = s...); }
fn k (s : any[]) : i64 { return h(s, 1, s...); }

pub fn main () : i64 { return 0; }

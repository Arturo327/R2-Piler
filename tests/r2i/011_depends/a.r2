module "depa";
depends "m", "z";
depends "m";

import "b";

pub fn main () : i64 { return b::fromb(); }

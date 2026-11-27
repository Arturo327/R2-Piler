module "hostm";
import "dep";

export fn ping (d : ptr dep::D) : i64 { return d.v + 1; }

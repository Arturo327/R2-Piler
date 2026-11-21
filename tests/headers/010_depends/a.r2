module "hdep";
depends "m", "z";
depends "m";

export fn f () : i64 { return 0; }

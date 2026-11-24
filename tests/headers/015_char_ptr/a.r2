module "chars";

export struct Str { s : ptr const i8; buf : i8[8]; n : i64; };

export var greeting : ptr const i8 = NULL;

export arr bytes : i8[8];

export var slice : i8[];

export fn first (s : ptr const i8) : i64 { return @s as i64; }

export fn touch (buf : i8[8]) : i64 { buf[0] = 65; return buf[0] as i64; }

export fn ret () : ptr const i8 { return NULL; }

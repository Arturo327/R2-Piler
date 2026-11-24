module "cvoid";

export struct Buf { p : ptr const void; q : ptr void; n : i64; };

export var ro : ptr const void = NULL;

export fn peek (p : ptr const void) : ptr const void { return p; }

export fn fill (p : ptr void, n : i64) : ptr void { if (n > 0) return p; return NULL; }

enum Dup {
    A = 1,
    A = 2,
}

enum Small : i8 {
    A = 200,
}

enum Tiny : u8 {
    A = 255,
    B,
}

enum Bad {
    A = 1 + 2,
}

enum Neg : u8 {
    A = -1,
}

enum Comp : u8 {
    A = ~0,
}

enum U64 : u64 {
    Max = ~0u,
    Wrap,
}

enum S64 : i64 {
    Min = ~0u,
}

enum Casts : u8 {
    A = (300 as u8),
    B,
    C = (-1 as u8),
    D,
}

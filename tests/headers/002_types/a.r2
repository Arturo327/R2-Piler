export struct Punto { x : i32; y : i32; };
export union U { big : i64; small : i32; };
export enum Color : u8 { Rojo = 1, Verde = 2 };
export type Entero = i64;
export newtype Manzana = u32;
export struct Bits { a : u8 : 3; b : u8 : 5; c : i16 : 4; d : i64; };
export packed struct Small { x : u8; y : i32; };
export struct Mixta { inner : Punto; p : ptr Punto; };

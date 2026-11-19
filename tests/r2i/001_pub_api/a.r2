module "api";

pub struct Punto {
	x : i32;
	y : i32;
}

pub union Dato {
	entero : i64;
}

pub enum Color : u8 {
	Red,
	Green = 5,
}

pub type Id = i64;
pub newtype Handle = u64;

pub var counter : i64 = 1;
pub arr table : i64[2] = {1, 2};

pub fn add (a : i32, b : i32 = 3) : i32
{
	return a + b;
}

pub fn main () : i64
{
	return 0;
}
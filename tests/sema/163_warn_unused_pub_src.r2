// 'pub' and 'export' declarations are externally visible: they never report
// unused. Names starting with '_' are explicitly allowed to be unused.

pub fn exported_fn () : void {}
pub var exported_var : i64 = 1;
export fn c_fn (x : i64) : i64 { return x; }
export var c_var : i64 = 2;
export arr c_arr : i64[2] = {1, 2};

pub struct PubStruct { unused : i64; }
pub enum PubEnum : u8 { Only }
pub type PubAlias = i64;
pub newtype PubNew = i64;

fn private_fn () : void {}
var private_var : i64 = 3;
struct PrivateStruct { a : i64; }

fn _muted_fn (_muted : i64) : void {}

pub fn main () : i64 {
	var _muted : i64 = 4;
	return 0;
}
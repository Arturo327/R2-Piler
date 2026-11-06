var g : i64 = 5;

fn v() : void { }

fn main() : i64 {
	var c : i64 = 1;
	var a : i64 = c ? 1 : "x";
	var b : i64 = c ? v() : 1;
	var d : i64 = c ? (1 as u64) : (2 as i32);
	var p : ptr i64 = c ? $g : 1;
	return 0;
}
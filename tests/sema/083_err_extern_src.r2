extern fn main () : i64;
extern var main : i64;

fn f () : i64 {
	extern var x : i64;
	return x;
}
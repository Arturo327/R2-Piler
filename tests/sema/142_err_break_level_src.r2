fn f() : void {
	while (1) {
		break 2;
	}
}

fn g() : void {
	while (1) {
		continue 0;
	}
}

fn main() : i64 {
	f();
	g();
	return 0;
}
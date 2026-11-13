fn f() : void {
	while (0) {
	} else {
		break;
	}
}

fn g() : void {
	var c : i64 = 0;

	while (c < 1) {
		c++;
	} else {
		continue;
	}
}

fn main() : i64 {
	f();
	g();
	return 0;
}
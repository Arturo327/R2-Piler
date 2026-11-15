pub fn main() : i64 {
	var acc = 0;
	var i = 1;
	while (i <= 10) {
		acc += i * 2;
		i += 1;
	}
	return acc;
}
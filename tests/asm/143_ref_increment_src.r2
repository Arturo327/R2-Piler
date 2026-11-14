fn incrementar(x : &i64, valor : i64) {
	x += valor;
}

fn main() : i64 {
	var numero : i64 = 10;
	incrementar(numero, 5);
	if (numero == 15) return 0;
	return 1;
}

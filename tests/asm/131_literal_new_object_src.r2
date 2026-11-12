struct P {
	x : i32;
	y : i32;
}

fn main () : i64 {
	var p : P = {.x = 1, .y = 2};
	var h : ptr P = $p;

	// un literal es un objeto nuevo: .x lee el p.y anterior (2), no el 5 recién escrito
	p = {.y = 5, .x = p.y} as P;
	if (p.x != 2) return 1;
	if (p.y != 5) return 2;

	// parcial: .y se zeroa, .x lee el p.x anterior
	p = {.x = p.x + 10} as P;
	if (p.x != 12) return 3;
	if (p.y != 0) return 4;

	var q : P = p;

	q = {.y = q.y + 1, .x = q.x} as P;
	if (q.x != 12) return 5;
	if (q.y != 1) return 6;

	// asignación a través de un puntero: destino no fresco
	@h = {.x = h.x + 100, .y = 77} as P;
	if (p.x != 112) return 7;
	if (p.y != 77) return 8;

	// elemento de array: destino no fresco
	var s : P[2];

	s[0] = {.x = 1, .y = 2} as P;
	s[1] = {.x = s[0].x + 1} as P;
	if (s[1].x != 2) return 9;
	if (s[1].y != 0) return 10;
	if (s[0].y != 2) return 11;

	return 0;
}

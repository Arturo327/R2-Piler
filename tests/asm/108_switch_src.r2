enum Dias : i64 {
	Lunes = 1,
	Martes = 2,
	Viernes = 5,
	Sabado = 6,
	Domingo = 7,
}

var g : i64 = 0;

fn bump(x : i64) : i64 {
	g = g + 1;
	return x;
}

fn day(d : Dias) : i64 {
	switch (d) {
	case (Dias.Viernes) {
		return 10;
	}
	case (Dias.Sabado, Dias.Domingo) {
		return 20;
	}
	default {
		return 30;
	}
	}
}

fn classify(n : i64) : i64 {
	var r : i64 = 0;

	switch (n) {
	case (1, 2) {
		r = 100;
	}
	case (3) {
		var local : i64 = 7;
		r = local;
	}
	default {
		r = 1;
	}
	}
	return r;
}

fn main() : i64 {
	if (day(Dias.Viernes) != 10) { return 1; }
	if (day(Dias.Sabado) != 20) { return 2; }
	if (day(Dias.Domingo) != 20) { return 3; }
	if (day(Dias.Lunes) != 30) { return 4; }

	if (classify(1) != 100) { return 5; }
	if (classify(2) != 100) { return 6; }
	if (classify(3) != 7) { return 7; }
	if (classify(9) != 1) { return 8; }

	g = 0;
	var z : i64 = 0;

	switch (bump(3)) {
	case (1, 2) { z = 1; }
	case (3) { z = 2; }
	default { z = 3; }
	}
	if (z != 2) { return 9; }
	if (g != 1) { return 10; }

	var n8 : i8 = 2;

	switch (n8) {
	case (-1) { z = 40; }
	case (2) { z = 41; }
	default { z = 42; }
	}
	if (z != 41) { return 11; }

	switch (n8) {
	case (2 + 3) { z = 50; }
	default { z = 51; }
	}
	if (z != 51) { return 12; }

	var e : i64 = 0;

	switch (e) {
	case (0) { }
	default { return 13; }
	}
	return 0;
}
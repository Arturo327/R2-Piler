enum Dias : i64 {
	Lunes = 1,
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
	switch (bump(3)) {
	case (1, 2) {
	}
	case (3) {
	}
	default {
	}
	}
	return day(Dias.Lunes) + classify(3);
}
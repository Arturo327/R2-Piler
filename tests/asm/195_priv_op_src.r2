class Acc {
	at : i64;
	total : i64;

	init (self, at : i64, total : i64) { self.at = at; self.total = total; }

	priv op + (self, b : Acc) : Acc {
		return Acc(self.at + b.at + 1000, 0);
	}

	priv op += (self, b : Acc) : void { self.total = self.total + b.at + 1; }

	fn combine(self, b : Acc) : Acc { return @self + b; }

	fn merge(self, b : Acc) : i64 {
		@self += b;
		return self.total;
	}
}

pub fn main() : i64
{
	var c : Acc = Acc(1, 10);
	var d : Acc = Acc(2, 0);
	var u : Acc = c.combine(d);
	var m : i64 = c.merge(d);
	return u.at + m;
}

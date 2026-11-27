class Counter {
	n : i64;

	init (self) { self.n = 0; }

	op ++ (self) : void { self.n = self.n + 2; }
	op -- (self) : void { self.n = self.n - 1; }
}

pub fn main() : i64
{
	var c : Counter = Counter();
	var d : Counter = Counter();

	c++;
	c++;
	c--;
	arr list : Counter[2];
	list[1] = Counter();
	list[1]++;
	for (var i : i64 = 0; i < 3; i++) { d++; }
	return c.n + list[1].n + d.n;
}

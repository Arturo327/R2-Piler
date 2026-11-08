struct Node {
	value: i64;
	next: ptr Node;
}

fn main() : i64 {
	var a : Node = {.value = 10, .next = NULL};
	var b : Node = {.value = 20, .next = $a};
	var p : ptr Node = $b;

	if (p.value != 20) return 1;
	if ((@p).value != 20) return 2;
	if (p.next.value != 10) return 3;
	if ((@p.next).value != 10) return 4;

	p.value = 30;
	b.value = b.value + 5;
	if (a.next != NULL) return 5;

	var q : ptr Node = p;

	q.next.value = 15;
	if (a.value != 15) return 6;
	if (b.value + a.value != 50) return 7;

	return 0;
}

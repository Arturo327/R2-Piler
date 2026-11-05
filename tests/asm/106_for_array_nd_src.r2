/* `for` de N dimensiones: un nivel por índice (`mat.len`, `mat[i].len`,
   `mat[i][j].len`, ...). Cada suma hecha con el bucle se compara con la hecha
   a mano con `while`, asi que un nivel mal anidado o un limite mal puesto no
   coinciden. */

arr mat : i32[3][4] = {
	{ 1, 2, 3, 4 },
	{ 5, 6, 7, 8 },
	{ 9, 10, 11, 12 },
};

arr cubo : i32[2][3][2] = {
	{ {1,2}, {3,4}, {5,6} },
	{ {7,8}, {9,10}, {11,12} },
};

fn manual () : i64
{
	var t : i64 = 0;
	var i : i64 = 0;

	while (i < mat.len) {
		var j : i64 = 0;
		while (j < mat[i].len) { t += mat[i][j] as i64; j++; }
		i++;
	}
	return t;
}

fn dos_tipos () : i64
{
	var t : i64 = 0;

	for (var i : i64, j : i64 : mat)
		t += mat[i][j] as i64;
	return t;
}

fn sin_tipos () : i64
{
	var t : i64 = 0;

	for (var i, j : mat)
		t += mat[i][j] as i64;
	return t;
}

fn con_vars () : i64
{
	var t : i64 = 0;
	var i : i64;
	var j : i64;

	for (i, j : mat)
		t += mat[i][j] as i64;
	return t;
}

/* cada elemento se visita exactamente una vez */
fn cuenta () : i64
{
	var n : i64 = 0;

	for (var i : i64, j : i64 : mat)
		n++;
	return n;
}

fn tres_d () : i64
{
	var t : i64 = 0;

	for (var i : i64, j : i64, k : i64 : cubo)
		t += cubo[i][j][k] as i64;
	return t;
}

fn mixto (m : i32[][4]) : i64
{
	var t : i64 = 0;

	for (var i : i64, j : i64 : m)
		t += m[i][j] as i64;
	return t;
}

/* la expresión iterada también puede ser una indexación: `c[1]` es i32[3][2] */
fn sub (c : i32[2][3][2]) : i64
{
	var t : i64 = 0;

	for (var j : i64, k : i64 : c[1])
		t += c[1][j][k] as i64;
	return t;
}

fn main () : i64
{
	var esperado : i64 = manual();
	var n : i64 = 12;

	if (dos_tipos() != esperado) return 1;
	if (sin_tipos() != esperado) return 2;
	if (con_vars() != esperado) return 3;
	if (cuenta() != n) return 4;
	if (tres_d() != 78) return 5;
	if (mixto(mat) != esperado) return 6;
	if (sub(cubo) != 57) return 7;

	return 0;
}

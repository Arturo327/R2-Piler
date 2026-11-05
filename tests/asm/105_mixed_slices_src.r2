/* Slices con dimensiones mezcladas: cada dimension escrita es estatica y
   cada `[]` es una longitud runtime que viaja en el descriptor tras el
   puntero. Las sumas hechas por el parametro mixto se comparan con la misma
   suma hecha por el array concreto (diferencial: un stride mal calculado no
   coincidiria). */

fn suma_fin (m : i32[2][3][]) : i64
{
	var t : i64 = 0;
	var k : i64 = 0;

	while (k < m[1][2].len) { t += m[1][2][k] as i64; k++; }
	return t;
}

fn suma_medio (m : i32[2][][4]) : i64
{
	var t : i64 = 0;
	var i : i64 = 0;

	while (i < m[0].len) {
		var j : i64 = 0;
		while (j < m[0][i].len) { t += m[1][i][j] as i64; j++; }
		i++;
	}
	return t;
}

fn suma_principio (m : i32[][2][3]) : i64
{
	var t : i64 = 0;
	var i : i64 = 0;

	while (i < m.len) {
		var j : i64 = 0;
		while (j < m[i].len) {
			var k : i64 = 0;
			while (k < m[i][j].len) { t += m[i][j][k] as i64; k++; }
			j++;
		}
		i++;
	}
	return t;
}

/* escribe a traves del parametro mixto: el caller ve el cambio */
fn escribe (m : i32[][3]) : i64
{
	m[1][2] = 60;
	return m[1][2] as i64;
}

fn suma_fila (v : i32[]) : i64 { return (v[0] + v[1] + v[2]) as i64; }

/* una fila de un mixto pasa como slice: ptr + la longitud de la dimension
   estatica de esa fila */
fn pasa_fila (m : i32[][3]) : i64 { return suma_fila(m[1]); }

fn dims (m : i32[2][][4]) : i64
{
	return m[0].len * 100 + m[0][0].len + m[1][m[0].len - 1][m[0][0].len - 1];
}

/* sizeof de un tipo con dimensiones dinamicas es el del descriptor */
fn descriptor (m : i32[][3], n : i32[2][], s : i32[][]) : i64
{
	return (sizeof(m) as i64) + (sizeof(n) as i64) + (sizeof(s) as i64)
			+ m[0][0] as i64 + n[0][0] as i64 + s[0][0] as i64;
}

fn main () : i64
{
	arr a : i32[2][3][2] = {
		{ {1,2}, {3,4}, {5,6} },
		{ {7,8}, {9,10}, {11,12} },
	};
	arr b : i32[2][5][4];
	arr c : i32[3][2][3];
	arr d : i32[2][3] = { { 1, 2, 3 }, { 4, 5, 6 } };
	var t : i64 = 0;
	var esperado : i64 = 0;
	var i : i64;

	i = 0;
	while (i < 2) {
		var j : i64 = 0;
		while (j < 5) {
			var k : i64 = 0;
			while (k < 4) { b[i][j][k] = (i * 100 + j * 10 + k) as i32; k++; }
			j++;
		}
		i++;
	}
	i = 0;
	while (i < 3) {
		var j : i64 = 0;
		while (j < 2) {
			var k : i64 = 0;
			while (k < 3) { c[i][j][k] = (i * 100 + j * 10 + k) as i32; k++; }
			j++;
		}
		i++;
	}

	t = suma_fin(a);
	esperado = 0;
	i = 0;
	while (i < 2) { esperado += a[1][2][i] as i64; i++; }
	if (t != esperado) return 1;

	t = suma_medio(b);
	esperado = 0;
	i = 0;
	while (i < 5) {
		var j : i64 = 0;
		while (j < 4) { esperado += b[1][i][j] as i64; j++; }
		i++;
	}
	if (t != esperado) return 2;

	t = suma_principio(c);
	esperado = 0;
	i = 0;
	while (i < 3) {
		var j : i64 = 0;
		while (j < 2) {
			var k : i64 = 0;
			while (k < 3) { esperado += c[i][j][k] as i64; k++; }
			j++;
		}
		i++;
	}
	if (t != esperado) return 3;

	if (pasa_fila(d) != 15) return 6;

	if (escribe(d) != 60) return 4;
	if (d[1][2] != 60) return 5;

	t = dims(b);
	esperado = 647;
	if (t != esperado) return 7;

	t = descriptor(d, d, d);
	esperado = 59;
	if (t != esperado) return 8;

	return 0;
}

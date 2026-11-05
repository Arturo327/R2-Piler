for (var i : i64, j : i64 : mat) {
  t += mat[i][j];
}
for (var a, b : mat) t += mat[a][b];
for (c, d : mat) {
  t += mat[c][d];
}
for (var x : i32, y : i32, z : i32 : cube) t += cube[x][y][z];
for (var i : i64 : m) t += m[i].len;

for (x; i < 10; i = i + 1) {
  x = 1;
}
for (foo(0); i < 10; i = i + 1) {
  x = 1;
}
for (-1; i < 10; i = i + 1) {
  x = 1;
}
for (i = 0; i + 2 * 3 < 10; i = i + 1) {
  x = 1;
}
for (i = 0; a && b || c; i = i + 1) {
  x = 1;
}
for (i = 0; foo(1) > 2; i = i + 1) {
  x = 1;
}
for (i = 0; i < 10; i = (i + 1) * 2) {
  x = 1;
}
for (i = 0; i < 10; x = y = i) {
  x = 1;
}

for (i = 0; i < 3; i = i + 1)
  for (j = 0; j < 3; j = j + 1)
    x = x + 1;
for (i = 0; i < 2; i = i + 1) {
  for (j = 0; j < 2; j = j + 1) {
    x = i + j;
  }
}
while (a) {
  for (i = 0; i < 3; i = i + 1) {
    x = x + i;
  }
}
for (i = 0; i < 3; i = i + 1) {
  while (b) {
    y = y - 1;
  }
}
if (x) {
  while (y) {
    y = y - 1;
  }
} else {
  for (i = 0; i < 3; i = i + 1) {
    x = x + i;
  }
}
for (i = 0; i < 10; i = i + 1) {
  if (i == 2) {
    x = 99;
  } else {
    x = x + i;
  }
}

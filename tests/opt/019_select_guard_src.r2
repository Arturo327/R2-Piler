var g : i64 = 0;
fn inner(a : i64) : i64 {
  var t : i64 = a + 1;
  return t;
}
fn bump(a : i64) : i64 {
  var u : i64 = inner(a);
  return u + 2;
}
fn f_global(c : i64) : i64 {
  if (c) {
    g = 1;
  } else {
    g = 2;
  }
  return g;
}
fn f_arith(c : i64, a : i64, b : i64) : i64 {
  var x : i64 = 0;
  if (c) {
    x = a + b;
  } else {
    x = 1;
  }
  return x;
}
fn f_call(c : i64, v : i64) : i64 {
  var x : i64 = 0;
  if (c) {
    x = bump(v);
  } else {
    x = 2;
  }
  return x;
}
fn f_elif(c : i64, d : i64) : i64 {
  var x : i64 = 0;
  if (c) {
    x = 1;
  } elif (d) {
    x = 2;
  } else {
    x = 3;
  }
  return x;
}
fn main() : i64 {
  return f_global(1) + f_arith(1, 2, 3) + f_call(1, 4) + f_elif(1, 0);
}

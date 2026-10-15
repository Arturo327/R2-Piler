fn lit(a : i64) : i64 {
  return a / 0;
}
fn modz(a : i64) : i64 {
  return a % 0;
}
fn folded(a : i64) : i64 {
  return a / (3 - 3);
}
fn casted(a : u8) : u8 {
  return a / (0 as u8);
}
fn ok_two(a : i64) : i64 {
  return a / 2;
}
fn ok_zero_num(a : i64) : i64 {
  return 0 / a;
}
fn ok_var_div(b : i64, c : i64) : i64 {
  return b / (c - c);
}

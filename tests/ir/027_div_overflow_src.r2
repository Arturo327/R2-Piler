fn d8() : i8 {
  return (-128 as i8) / (-1 as i8);
}
fn d8b() : i8 {
  return (-128 as i8) / -1;
}
fn m8() : i8 {
  return (-128 as i8) % (-1 as i8);
}
fn ok8() : i8 {
  return (-127 as i8) / (-1 as i8);
}
fn d16() : i16 {
  return (-32768 as i16) / (-1 as i16);
}
fn d64() : i64 {
  return (0x8000000000000000u as i64) / (-1 as i64);
}
fn u8div() : u8 {
  return (255 as u8) / (255 as u8);
}
fn rt(a : i16) : i16 {
  return a / (-1 as i16);
}

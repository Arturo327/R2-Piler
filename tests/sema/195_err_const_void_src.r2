pub fn bad () : const void { return; }

pub fn main () : i64 {
  return sizeof(const void) as i64;
}

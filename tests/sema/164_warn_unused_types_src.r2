// Types that are never referenced are reported. An unused type does not also
// report every one of its fields. Names starting with '_' are muted.

struct Used { a : i64; b : i64; }
struct Dead { x : i64; y : i64; }
union DeadUnion { i : i64; f : u32; }
enum DeadEnum : u8 { One, Two }
type DeadAlias = i64;
newtype DeadNew = i64;
type _MutedAlias = i64;

pub fn main () : i64 {
	var u : Used = {.a = 1, .b = 2};
	return u.a + u.b;
}
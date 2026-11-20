import lib;
from lib import c_entry;

pub fn main () : i64
{
	lib::c_count = 3;
	return lib::c_entry(1) as i64 + c_entry(2) as i64 + lib::abs(-4) + lib::c_count;
}

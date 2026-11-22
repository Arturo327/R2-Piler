import "lib";

pub fn main() : i64 {
    var h : lib::Handle = lib::make(17);
    return lib::read(h);
}

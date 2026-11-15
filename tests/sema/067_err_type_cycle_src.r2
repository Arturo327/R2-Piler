type A = B;
type B = A;
type Self = Self;

newtype NA = NB;
newtype NB = NA;

pub fn main () : i64 {
    return 0;
}

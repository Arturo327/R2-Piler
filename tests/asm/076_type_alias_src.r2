type Count = u32;
type Index = Count;

fn sum (n : Index) : Index {
    var i : Index = 0;
    var acc : Index = 0;
    while (i < n) {
        acc += i;
        i++;
    }
    return acc;
}

fn main () : i64 {
    var total : Index = sum(100);
    return (total as i64) % 256;
}

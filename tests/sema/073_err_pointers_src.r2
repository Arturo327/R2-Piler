fn main() : i64 {
    var x32 : i32 = 42;
    var x : i64 = 7;
    var p : ptr i32 = $x32;
    var n : ptr void = NULL;

    var a : i64 = @n;            // cannot dereference 'ptr void'
    var b : ptr i32 = n + 1;     // pointer arithmetic on 'ptr void'
    var c : i64 = p == 0;        // cannot compare a pointer with an integer
    var d : i64 = p as i64;      // cannot cast between pointer and non-pointer
    var e : ptr i64 = 5 as ptr i64; // cannot cast between pointer and non-pointer
    var f : ptr i32 = 7;         // cannot assign untyped int to a variable of type ptr i32
    var g : i64 = @x;            // cannot dereference a value of type i64
    var h : ptr i32 = p * 2;     // operator cannot be applied to a value of type ptr i32
    var i : ptr i32 = n + NULL;  // cannot use NULL in pointer arithmetic
    return 0;
}

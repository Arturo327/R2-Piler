struct V { a : i64[3]; s : i64; }

arr g : i64[4] = {1, 2, 3, 4};

fn w_arr() : i64[] {
    arr x : i64[3] = {1, 2, 3};
    return x;
}

fn w_slice() : i64[] {
    arr x : i64[3] = {1, 2, 3};
    var s : i64[] = x;
    return s;
}

fn w_chain() : i64[] {
    arr x : i64[2] = {1, 2};
    var s : i64[] = x;
    var t : i64[] = s;
    return t;
}

fn w_sub() : i64[] {
    arr m : i64[2][3] = {{1, 2, 3}, {4, 5, 6}};
    return m[0];
}

fn w_field() : i64[] {
    var v : V;
    return v.a;
}

fn w_addr() : ptr i64 {
    var x : i64 = 1;
    return $x;
}

fn ok_global() : i64[] {
    return g;
}

fn ok_param(x : i64[]) : i64[] {
    return x;
}

fn ok_arr_param(x : i64[3]) : i64[3] {
    return x;
}

fn main() : i64 {
    return 0;
}
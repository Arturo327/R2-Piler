pub newtype Handle = ptr i64;

var cell : i64;

pub fn make (v : i64) : Handle {
    cell = v;
    return $cell as Handle;
}

pub fn read (h : Handle) : i64 { return @h; }

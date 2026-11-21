extern fn r2_note_init ();
extern fn r2_get_calls () : i64;

fn seed () : i64 { r2_note_init(); return 41; }

pub var g : i64 = seed();

pub fn get () : i64 { return g; }
pub fn calls () : i64 { return r2_get_calls(); }

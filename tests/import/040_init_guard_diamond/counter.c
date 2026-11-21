/* Cuenta cuántas veces se ejecuta realmente el init de `a`. */
int r2_init_calls = 0;

void r2_note_init (void) { r2_init_calls++; }
int r2_get_calls (void) { return r2_init_calls; }

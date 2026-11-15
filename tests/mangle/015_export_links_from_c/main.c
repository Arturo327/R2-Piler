#include <stdint.h>
extern int64_t callback (int64_t x);
extern int64_t contador;
extern int64_t tabla[3];
int main (void) { return (int)(callback(contador) + tabla[1]); }

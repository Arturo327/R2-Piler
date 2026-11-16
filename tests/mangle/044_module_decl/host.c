extern void __r2_init_5math2 (void);
extern long _r2_pf_5math2_3add (long a, long b);

int main (void)
{
	__r2_init_5math2();
	return (int)_r2_pf_5math2_3add(20, 22);
}

#include "zz_priv.h"

extern void zz_c_test(void);

int main(int argc, char **argv)
{
	(void)argc;
	(void)argv;

	zz_c_test();
	return 0;
}

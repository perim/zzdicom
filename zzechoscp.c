#include "zznet.h"

int main(void)
{
	struct zzfile szz, *zz;
	zz = zznetwork(NULL, "ECHOSCU", &szz);
	zzlisten(zz, 5104, "TEST", 0);
	return 0;
}

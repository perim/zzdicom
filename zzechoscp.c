#include "zznet.h"

int main(int argc, char **argv)
{
	struct zzfile szz, *zz;
	zz = zznetwork(NULL, "zzdicom", &szz);
	zzlisten(zz, 5104, "TEST", 0);
	return 0;
}

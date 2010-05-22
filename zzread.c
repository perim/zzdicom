#include "zz_priv.h"
#include "zzsql.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

int main(int argc, char **argv)
{
	struct zzdb *zdb;
	int i;

	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s <filenames>\n", argv[0]);
		exit(-1);
	}
	zdb = zzdbopen();
	if (!zdb)
	{
		fprintf(stderr, "Error opening local DICOM database - aborting.\n");
		exit(-1);
	}
	for (i = 1; i < argc; i++)
	{
		struct zzfile *zz = zzopen(argv[i], "r");
		zzdbupdate(zdb, zz);
		zz = zzclose(zz);
	}
	zdb = zzdbclose(zdb);

	return 0;
}

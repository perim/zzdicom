#include "zz_priv.h"
#include "zzsql.h"

int main(int argc, char **argv)
{
	struct zzdb *zdb;
	int i;

	zdb = zzdbopen();
	if (!zdb)
	{
		fprintf(stderr, "Error opening local DICOM database - aborting.\n");
		exit(-1);
	}
	for (i = zzutil(argc, argv, 2, "<filenames>", "Add files to local DICOM database"); i < argc; i++)
	{
		struct zzfile *zz = zzopen(argv[i], "r");
		if (zz)
		{
			zzdbupdate(zdb, zz);
			zz = zzclose(zz);
		}
	}
	zdb = zzdbclose(zdb);

	return 0;
}

#include "zz_priv.h"
#include "zzsql.h"

int main(int argc, char **argv)
{
	struct zzdb szdb, *zdb;
	struct zzfile szz, *zz;
	int i;

	zdb = zzdbopen(&szdb);
	if (!zdb)
	{
		fprintf(stderr, "Error opening local DICOM database - aborting.\n");
		exit(-1);
	}
	for (i = zzutil(argc, argv, 2, "<filenames>", "Add files to local DICOM database", NULL); i < argc; i++)
	{
		zz = zzopen(argv[i], "r", &szz);
		if (zz)
		{
			zzdbupdate(zdb, zz);
			zz = zzclose(zz);
		}
	}
	zdb = zzdbclose(zdb);

	return 0;
}

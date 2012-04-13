#include "zz_priv.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

void fix(char *filename)
{
	struct zzfile szz, *zz;
	uint16_t group, element, lastgroup = 0xffff;
	long len, groupsize = 0, grouppos = 0;

	zz = zzopen(filename, "r", &szz);

	zziterinit(zz);
	while (zziternext(zz, &group, &element, &len))
	{
		if (group != lastgroup)
		{
			if (groupsize != 0)
			{
				if (zireadpos(zz->zi) - grouppos != groupsize)
				{
					uint32_t cur = zireadpos(zz->zi);

					fprintf(stdout, "Wrong group %x size - told it was %ld, but it was %ld - fixing!\n", group, groupsize, zireadpos(zz->zi) - grouppos);
					zisetreadpos(zz->zi, grouppos - 4);
					ziwrite(zz->zi, &grouppos, 4);
					zisetreadpos(zz->zi, cur);
				}
			}

			if (element == 0x0000)
			{
				groupsize = zzgetuint32(zz, 0);
				zisetreadpos(zz->zi, zireadpos(zz->zi) - 4);
			}
			groupsize = 0;
			grouppos = zireadpos(zz->zi);
			lastgroup = group;
		}
	}
	zz = zzclose(zz);
}

int main(int argc, char **argv)
{
	int i;

	for (i = zzutil(argc, argv, 1, "<filenames>", "DICOM group size fixer", NULL); i < argc; i++)
	{
		fix(argv[i]);
	}

	return 0;
}

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

	while (zz && !feof(zz->fp) && !ferror(zz->fp))
	{
		zzread(zz, &group, &element, &len);

		if (group != lastgroup)
		{
			if (groupsize != 0)
			{
				if (ftell(zz->fp) - grouppos != groupsize)
				{
					uint32_t cur = ftell(zz->fp);

					fprintf(stdout, "Wrong group %x size - told it was %ld, but it was %ld - fixing!\n", group, groupsize, ftell(zz->fp) - grouppos);
					fseek(zz->fp, grouppos - 4, SEEK_SET);
					fwrite(&grouppos, 4, 1, zz->fp);
					fseek(zz->fp, cur, SEEK_SET);
				}
			}

			if (element == 0x0000)
			{
				groupsize = zzgetuint32(zz);
				fseek(zz->fp, -4, SEEK_CUR);
			}
			groupsize = 0;
			grouppos = ftell(zz->fp);
			lastgroup = group;
		}

		// Abort early, skip loading pixel data into memory if possible
		if ((uint32_t)ftell(zz->fp) + len == zz->fileSize)
		{
			break;
		}

		// Skip ahead
		if (!feof(zz->fp) && len != 0xFFFFFFFF && len > 0)
		{
			fseek(zz->fp, len, SEEK_CUR);
		}
	}
	zz = zzclose(zz);
}

int main(int argc, char **argv)
{
	int i;

	for (i = zzutil(argc, argv, 2, "<filenames>", "DICOM group size fixer"); i < argc; i++)
	{
		fix(argv[i]);
	}

	return 0;
}

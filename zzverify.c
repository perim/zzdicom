#include "zz_priv.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

// Verifies:
//  - group lengths
//  - groups in ascending order
//  - elements in ascending order

void dump(char *filename)
{
	struct zzfile *zz;
	uint16_t group, element, lastgroup = 0xffff, lastelement = 0;
	uint32_t len, size, groupsize = 0;
	struct stat st;
	int grouppos = 0;

	zz = zzopen(filename, "r");
	if (!zz)
	{
		fprintf(stderr, "Failed to open %s\n", filename);
		exit(-1);
	}
	fstat(fileno(zz->fp), &st);
	size = st.st_size;

	while (!feof(zz->fp) && !ferror(zz->fp))
	{
		zzread(zz, &group, &element, &len);

		if (group != lastgroup)
		{
			if (groupsize != 0)
			{
				if (ftell(zz->fp) - grouppos != groupsize)
				{
					fprintf(stderr, "Wrong group %x size - told it was %u, but it was %ld\n", group, groupsize, ftell(zz->fp) - grouppos);
				}
			}

			if (element == 0x0000)
			{
				groupsize = zzgetuint32(zz);
				fseek(zz->fp, -4, SEEK_CUR);
			}
			groupsize = 0;
			grouppos = ftell(zz->fp);
			if (lastgroup != 0xffff && lastgroup != 0xfffe && group < lastgroup)
			{
				fprintf(stderr, "Group 0x%04x - order not ascending (last is 0x%04x)!\n", group, lastgroup);
			}
			lastgroup = group;
		}
		if (element < lastelement)
		{
			fprintf(stderr, "Element %x - order not ascending!\n", element);
		}

		if (len > 0 && len != 0xFFFFFFFF)
		{
			int pos = ftell(zz->fp);

			if (pos + len > size)
			{
				fprintf(stderr, "(0x%04x,0x%04x) -- size exceeds file end\n", group, element);
			}

			// Abort early, skip loading pixel data into memory if possible
			if (pos + len >= size)
			{
				break;
			}

			// Skip ahead
			if (!feof(zz->fp))
			{
				fseek(zz->fp, len, SEEK_CUR);
			}
		}
	}
	zz = zzclose(zz);
}

int main(int argc, char **argv)
{
	int i;

	if (argc < 2)
	{
		fprintf(stderr, "Usage: %s <filenames>\n", argv[0]);
		exit(-1);
	}
	for (i = 1; i < argc; i++)
	{
		dump(argv[i]);
	}

	return 0;
}

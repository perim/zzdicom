#include "zz_priv.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

void fix(char *filename)
{
	struct zzfile *zz;
	uint16_t group, element, lastgroup = 0xffff;
	uint32_t len, size, groupsize = 0, grouppos = 0;
	const struct part6 *tag;
	struct stat st;

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
					uint32_t cur = ftell(zz->fp);

					fprintf(stdout, "Wrong group %x size - told it was %u, but it was %ld - fixing!\n", group, groupsize, ftell(zz->fp) - grouppos);
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
		tag = zztag(group, element);

		// Abort early, skip loading pixel data into memory if possible
		if (ftell(zz->fp) + len == size)
		{
			break;
		}

		// Skip ahead
		if (!feof(zz->fp) && len != 0xFFFFFFFF && len > 0)
		{
			fseek(zz->fp, len, SEEK_CUR);
		}
	}
	zz = zzfree(zz);
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
		fix(argv[i]);
	}

	return 0;
}

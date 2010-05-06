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
	FILE *fp;
	uint16_t group, element, lastgroup = 0xffff, lastelement = 0;
	uint32_t len, size, groupsize = 0;
	const struct part6 *tag;
	struct stat st;
	int grouppos = 0;

	fp = zzopen(filename, "r");
	if (!fp)
	{
		fprintf(stderr, "Failed to open %s\n", filename);
		exit(-1);
	}
	fstat(fileno(fp), &st); 
	size = st.st_size;

	while (!feof(fp) && !ferror(fp))
	{
		zzread(fp, &group, &element, &len);

		if (group != lastgroup)
		{
			if (groupsize != 0)
			{
				if (ftell(fp) - grouppos != groupsize)
				{
					fprintf(stderr, "Wrong group %x size - told it was %u, but it was %ld\n", group, groupsize, ftell(fp) - grouppos);
				}
			}

			if (element == 0x0000)
			{
				groupsize = zzgetuint32(fp);
				fseek(fp, -4, SEEK_CUR);
			}
			groupsize = 0;
			grouppos = ftell(fp);
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

		tag = zztag(group, element);

		// Abort early, skip loading pixel data into memory if possible
		if (ftell(fp) + len == size)
		{
			break;
		}

		// Skip ahead
		if (!feof(fp) && len != 0xFFFFFFFF && len > 0)
		{
			fseek(fp, len, SEEK_CUR);
		}
	}
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

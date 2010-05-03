#include "zz_priv.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

void dump(char *filename)
{
	FILE *fp;
	uint16_t group, element;
	uint32_t len, size;
	const struct part6 *tag;
	struct stat st;

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

		tag = zztag(group, element);

		// Presenting in DCMTK's syntax
		printf("(%04x,%04x) %-42s # %4d, %s %s\n", tag->group, tag->element, tag->VR, len, tag->VM, tag->description);

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

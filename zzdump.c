#include "zz_priv.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define MAX_LEN_VALUE 40

void dump(char *filename)
{
	FILE *fp;
	uint16_t group, element;
	uint32_t len, size, pos;
	const struct part6 *tag;
	struct stat st;
	char value[MAX_LEN_VALUE];

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

		pos = ftell(fp);
		tag = zztag(group, element);

		if (!tag)
		{
			printf("(%04x,%04x) -- unknown tag\n", group, element);
			if (!feof(fp) && len != 0xFFFFFFFF && len > 0)
			{
				fseek(fp, len, SEEK_CUR);
			}
			continue;
		}
		memset(value, 0, sizeof(value));
		if (tag->VR[0] == 'U' && tag->VR[1] == 'L')
		{
			snprintf(value, sizeof(value) - 1, "%u", zzgetuint32(fp));
		}
		else if (tag->VR[0] == 'U' && tag->VR[1] == 'S')
		{
			snprintf(value, sizeof(value) - 1, "%u", zzgetuint16(fp));
		}
		else if (tag->VR[0] == 'S' && tag->VR[1] == 'S')
		{
			snprintf(value, sizeof(value) - 1, "%u", zzgetint16(fp));
		}
		else if (tag->VR[0] == 'S' && tag->VR[1] == 'L')
		{
			snprintf(value, sizeof(value) - 1, "%u", zzgetint32(fp));
		}
		else if ((tag->VR[0] == 'L' && tag->VR[1] == 'O') || (tag->VR[0] == 'S' && tag->VR[1] == 'H')
		         || (tag->VR[0] == 'C' && tag->VR[1] == 'S') || (tag->VR[0] == 'D' && tag->VR[1] == 'S')
		         || (tag->VR[0] == 'A' && tag->VR[1] == 'E') || (tag->VR[0] == 'P' && tag->VR[1] == 'N')
		         || (tag->VR[0] == 'U' && tag->VR[1] == 'I') || (tag->VR[0] == 'L' && tag->VR[1] == 'T')
		         || (tag->VR[0] == 'A' && tag->VR[1] == 'S') || (tag->VR[0] == 'D' && tag->VR[1] == 'T')
		         || (tag->VR[0] == 'I' && tag->VR[1] == 'S') || (tag->VR[0] == 'U' && tag->VR[1] == 'T')
		         || (tag->VR[0] == 'T' && tag->VR[1] == 'M') || (tag->VR[0] == 'D' && tag->VR[1] == 'A'))
		{
			fread(value, MIN(len, sizeof(value) - 1), 1, fp);
		}

		// Presenting in DCMTK's syntax
		printf("(%04x,%04x) %-5s %-42s # %4d, %s %s\n", tag->group, tag->element, tag->VR, value, len, tag->VM, tag->description);

		// Abort early, skip loading pixel data into memory if possible
		if (ftell(fp) + len == size)
		{
			break;
		}

		// Skip ahead
		if (!feof(fp) && len != 0xFFFFFFFF && len > 0)
		{
			fseek(fp, pos + len, SEEK_SET);
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

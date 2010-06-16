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
	struct zzfile *zz;
	uint16_t group, element;
	uint32_t len, size, pos;
	const struct part6 *tag;
	struct stat st;
	char value[MAX_LEN_VALUE];

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

		pos = ftell(zz->fp);
		tag = zztag(group, element);

		if (!tag)
		{
			printf("(%04x,%04x) -- unknown tag\n", group, element);
			if (!feof(zz->fp) && len != 0xFFFFFFFF && len > 0)
			{
				fseek(zz->fp, len, SEEK_CUR);
			}
			continue;
		}
		memset(value, 0, sizeof(value));
		if (tag->VR[0] == 'U' && tag->VR[1] == 'L')
		{
			snprintf(value, sizeof(value) - 1, "%u", zzgetuint32(zz));
		}
		else if (tag->VR[0] == 'U' && tag->VR[1] == 'S')
		{
			snprintf(value, sizeof(value) - 1, "%u", zzgetuint16(zz));
		}
		else if (tag->VR[0] == 'S' && tag->VR[1] == 'S')
		{
			snprintf(value, sizeof(value) - 1, "%u", zzgetint16(zz));
		}
		else if (tag->VR[0] == 'S' && tag->VR[1] == 'L')
		{
			snprintf(value, sizeof(value) - 1, "%u", zzgetint32(zz));
		}
		else if ((tag->VR[0] == 'L' && tag->VR[1] == 'O') || (tag->VR[0] == 'S' && tag->VR[1] == 'H')
		         || (tag->VR[0] == 'C' && tag->VR[1] == 'S') || (tag->VR[0] == 'D' && tag->VR[1] == 'S')
		         || (tag->VR[0] == 'A' && tag->VR[1] == 'E') || (tag->VR[0] == 'P' && tag->VR[1] == 'N')
		         || (tag->VR[0] == 'U' && tag->VR[1] == 'I') || (tag->VR[0] == 'L' && tag->VR[1] == 'T')
		         || (tag->VR[0] == 'A' && tag->VR[1] == 'S') || (tag->VR[0] == 'D' && tag->VR[1] == 'T')
		         || (tag->VR[0] == 'I' && tag->VR[1] == 'S') || (tag->VR[0] == 'U' && tag->VR[1] == 'T')
		         || (tag->VR[0] == 'T' && tag->VR[1] == 'M') || (tag->VR[0] == 'D' && tag->VR[1] == 'A'))
		{
			fread(value, MIN(len, sizeof(value) - 1), 1, zz->fp);
		}

		// Presenting in DCMTK's syntax
		printf("(%04x,%04x) %-5s %-42s # %4d, %s %s\n", tag->group, tag->element, tag->VR, value, len, tag->VM, tag->description);

		// Abort early, skip loading pixel data into memory if possible
		if (ftell(zz->fp) + len == size)
		{
			break;
		}

		// Skip ahead
		if (!feof(zz->fp) && len != 0xFFFFFFFF && len > 0)
		{
			fseek(zz->fp, pos + len, SEEK_SET);
		}
	}
	zz = zzclose(zz);
}

int main(int argc, char **argv)
{
	int i;

	for (i = zzutil(argc, argv, 2, "<filenames>", "DICOM tag dumper"); i < argc; i++)
	{
		dump(argv[i]);
	}

	return 0;
}

#include "zz_priv.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define MAX_FILL 256

// List the below tags in sequential order, since we optimize the search this way.
static zzKey taglist[] = { ZZ_KEY(0x0010, 0x0010), ZZ_KEY(0x0010, 0x0020), ZZ_KEY(0x0010, 0x0030) };

static char fill[MAX_FILL];

void anonymize(char *filename)
{
	struct zzfile szz, *zz;
	uint16_t group, element, nexttag = 0;
	long len;
	int i;

	zz = zzopen(filename, "r+", &szz);

	while (zz && !feof(zz->fp) && nexttag < ARRAY_SIZE(taglist))
	{
		zzread(zz, &group, &element, &len);

		if (group > 0x0040 && group != 0xfffe)
		{
			break;	// no reason to look into groups we do not change
		}
		else if (group % 2 == 0 && element % 2 == 0)	// disregard private tags
		{
			// Match against our list of tags
			for (i = 0; i < (long)ARRAY_SIZE(taglist); i++)
			{
				if (group == ZZ_GROUP(taglist[i]) && element == ZZ_ELEMENT(taglist[i]))
				{
					const struct part6 *tag = zztag(group, element);

					if (tag->VR[0] == 'D' && tag->VR[1] == 'A')
					{
						const char *dstr = "19000101";

						fwrite(dstr, MIN(len, (long)strlen(dstr)), 1, zz->fp);
					}
					else
					{
						fwrite(fill, len, 1, zz->fp);
					}
					len = 0;			// do not seek further
					nexttag++;			// abort early when all items found
				}
			}
		}

		// Skip ahead
		if (len != UNLIMITED && len > 0)
		{
			fseek(zz->fp, len, SEEK_CUR);
		}
	}
	zz = zzclose(zz);
}

int main(int argc, char **argv)
{
	int i, ignparams;

	ignparams = zzutil(argc, argv, 3, "<replacement text> <filename>", "DICOM anonymization program");
	fill[0] = '\0';
	strcpy(fill, argv[1]);
	memset(fill + strlen(argv[ignparams]), '#', sizeof(fill));
	for (i = ignparams + 1; i < argc; i++)
	{
		anonymize(argv[i]);
	}

	return 0;
}

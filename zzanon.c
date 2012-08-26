#include "zz_priv.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define MAX_FILL 256

// List the below tags in sequential order, since we optimize the search this way.
static zzKey taglist[] = { ZZ_KEY(0x0010, 0x0010), ZZ_KEY(0x0010, 0x0020), ZZ_KEY(0x0010, 0x0030), ZZ_KEY(0x0010, 0x0032), ZZ_KEY(0x0010, 0x1000), ZZ_KEY(0x0010, 0x1001), ZZ_KEY(0x0010, 0x1005), ZZ_KEY(0x0010, 0x1040) };
static const char *tagvrs[] = { "PN", "LO", "DA", "TM", "LO", "PN", "PN", "LO" };

static char fill[MAX_FILL];

void anonymize(char *filename)
{
	struct zzfile szz, *zz;
	int group, element, nexttag = 0;
	long len;
	int i;

	zz = zzopen(filename, "r+", &szz);
	if (!zz)
	{
		exit(1);
	}

	zziterinit(zz);
	while (zziternext(zz, &group, &element, &len) && nexttag < (int)ARRAY_SIZE(taglist))
	{
		if (group < 0)
		{
			continue; // ignore fake delimeters
		}
		else if (group > 0x0040 && group != 0xfffe)
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
					zisetwritepos(zz->zi, zireadpos(zz->zi));
					if (tagvrs[i][0] == 'D' && tagvrs[i][1] == 'A')
					{
						const char *dstr = "19000101";

						ziwrite(zz->zi, dstr, MIN(len, (long)strlen(dstr)));
					}
					else
					{
						ziwrite(zz->zi, fill, len);
					}
					len = 0;			// do not seek further
					nexttag++;			// abort early when all items found
				}
			}
		}
	}
	zz = zzclose(zz);
}

int main(int argc, char **argv)
{
	int i, ignparams;

	ignparams = zzutil(argc, argv, 2, "<replacement text> <filenames>", "DICOM anonymization program", NULL);
	fill[0] = '\0';
	strcpy(fill, argv[ignparams]);
	memset(fill + strlen(argv[ignparams]), '#', sizeof(fill) - strlen(argv[ignparams]));
	for (i = ignparams + 1; i < argc; i++)
	{
		anonymize(argv[i]);
	}

	return 0;
}

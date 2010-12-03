#include "zz_priv.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define MAX_LEN_VALUE 42

static const char *txsyn2str(enum zztxsyn syn)
{
	switch (syn)
	{
	case ZZ_EXPLICIT: return "Little Endian Explicit";
	case ZZ_IMPLICIT: return "Little Endian Implicit";
	case ZZ_EXPLICIT_COMPRESSED: return "Deflated Explicit Little Endian";	// switched word order is in standard too
	}
	return "Internal Error";
}

void dump(char *filename)
{
	struct zzfile szz, *zz;
	uint16_t group, element;
	long len;
	const struct part6 *tag;
	char value[MAX_LEN_VALUE];
	char tmp[MAX_LEN_VALUE], vrstr[MAX_LEN_VR];
	int i;
	int header = 0;		// 0 - started, 1 - writing header, 2 - written header

	zz = zzopen(filename, "r", &szz);

	if (zz)
	{
		printf("\n# Dicom-File-Format\n");
	}

	zziterinit(zz);
	while (zziternext(zz, &group, &element, &len))
	{
		enum VR vr = zz->current.vr;
		tag = zztag(group, element);

		if (zz->part10 && header == 0)
		{
			printf("\n# Dicom-Meta-Information-Header\n");
			printf("# Used TransferSyntax: %s\n", txsyn2str(zz->ladder[1].txsyn));
			header = 1;
		}
		else if (zz->current.group > 0x0002 && header < 2)
		{
			printf("\n# Dicom-Data-Set\n");
			printf("# Used TransferSyntax: %s\n", txsyn2str(zz->ladder[0].txsyn));
			header = 2;
		}

		memset(value, 0, sizeof(value));		// need to zero all first
		strcpy(value, "(unknown value format)");	// for implicit and no dictionary entry

		for (i = 0; i < zz->currNesting; i++) printf("  ");

		zztostring(zz, value, sizeof(value));

		if (group > 0x0002 && element == 0x0000)	// generic group length
		{
			printf("(%04x,%04x) UL %-42s # %4ld, 1 Generic Group Length\n", group, element, value, len);
			continue;
		}
		else if (group % 2 > 0 && element < 0x1000 && len != UNLIMITED)
		{
			printf("(%04x,%04x) LO %-42s # %4ld, 1 Private Creator\n", group, element, value, len);
			continue;
		}
		else if (tag && zz->current.vr == NO && group != 0xfffe)
		{
			vr = ZZ_VR(tag->VR[0], tag->VR[1]);
		}

		if (len == UNLIMITED)
		{
			strcpy(tmp, "u/l");
		}
		else
		{
			snprintf(tmp, sizeof(tmp) - 1, "%ld", len);
		}

		// Presenting in DCMTK's syntax
		printf("(%04x,%04x) %s %-42s # %4s, %s %s\n", group, element, zzvr2str(vr, vrstr), value, tmp, tag ? tag->VM : "?", tag ? tag->description : "?");
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

#include "zz_priv.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "part6.h"

#define MAX_LEN_VALUE 42

static const char *txsyn2str(enum zztxsyn syn)
{
	switch (syn)
	{
	case ZZ_EXPLICIT: return "Little Endian Explicit";
	case ZZ_IMPLICIT: return "Little Endian Implicit";
	case ZZ_EXPLICIT_COMPRESSED: return "Deflated Explicit Little Endian";	// switched word order is in standard too
	case ZZ_EXPLICIT_JPEGLS: return "JPEG-LS Lossless Image Compression";
	case ZZ_TX_LAST: break;
	}
	return "Internal Error";
}

void dump(char *filename)
{
	struct zzfile szz, *zz;
	uint16_t group, element;
	long len;
	const char *vm, *description;
	const struct part6 *tag;
	const struct privatedic *privtag;
	char value[MAX_LEN_VALUE];
	char privcreator[MAX_LEN_VALUE];
	char tmp[MAX_LEN_VALUE], vrstr[MAX_LEN_VR];
	int i, privoffset = 0;
	int header = 0;		// 0 - started, 1 - writing header, 2 - written header
	char extra[10];

	zz = zzopen(filename, "r", &szz);

	if (zz)
	{
		printf("\n# Dicom-File-Format\n");
	}

	memset(privcreator, 0, sizeof(privcreator));
	zziterinit(zz);
	while (zziternext(zz, &group, &element, &len))
	{
		// Extra checks
		zzverify(zz);

		// Dictionary magic
		vm = "?";
		description = "?";
		if (group % 2 == 0)
		{
			tag = zztag(group, element);
			if (tag && zz->ladder[zz->ladderidx].txsyn == ZZ_IMPLICIT && group != 0xfffe)
			{
				zz->current.vr = ZZ_VR(tag->VR[0], tag->VR[1]);
			}
			if (tag)
			{
				vm = tag->VM;
				description = tag->description;
			}
		}
		else
		{
			privtag = zzprivtag(group, element, privcreator, privoffset);
			if (privtag && zz->ladder[zz->ladderidx].txsyn == ZZ_IMPLICIT && group != 0xfffe)
			{
				zz->current.vr = ZZ_VR(privtag->VR[0], privtag->VR[1]);
			}
			if (privtag)
			{
				vm = privtag->VM;
				description = privtag->description;
			}
		}

		// Pretty print headers
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

		memset(extra, 0, sizeof(extra));
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
			// TODO: Handle multiple private creator groups somehow
			printf("(%04x,%04x) LO %-42s # %4ld, 1 Private Creator\n", group, element, value, len);
			zz->current.vr = LO;
			privoffset = element * 0x100;
			fseek(zz->fp, zz->current.pos, SEEK_SET);
			zzgetstring(zz, privcreator, sizeof(privcreator) - 1);
			continue;
		}

		if (len == UNLIMITED)
		{
			strcpy(tmp, "u/l");
		}
		else
		{
			snprintf(tmp, sizeof(tmp) - 1, "%ld", len);
		}

		if (ZZ_KEY(group, element) == DCM_Item)
		{
			snprintf(extra, sizeof(extra) - 1, " %d [%d]", zz->ladder[zz->ladderidx].item + 1, zz->ladderidx);
		}

		// Presenting in DCMTK's syntax
		printf("(%04x,%04x) %s %-42s # %4s, %s %s%s\n", group, element, zzvr2str(zz->current.vr, vrstr), value, tmp, vm, description, extra);

		if (!zz->current.valid)
		{
			for (i = 0; i < zz->currNesting; i++) printf("  ");
			printf("^^ Warning: %s\n", zz->current.warning);
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

#include "zz_priv.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define MAX_LEN_VALUE 42

// Not implemented as reversing the bitshifting of the enum definition because this way it is reentrant
static const char *vr2str(enum VR vr)
{
	switch (vr)
	{
	case AE: return "AE";
	case AS: return "AS";
	case AT: return "AT";
	case CS: return "CS";
	case DA: return "DA";
	case DS: return "DS";
	case DT: return "DT";
	case FL: return "FL";
	case FD: return "FD";
	case IS: return "IS";
	case LO: return "LO";
	case LT: return "LT";
	case OB: return "OB";
	case OW: return "OW";
	case OF: return "OF";
	case PN: return "PN";
	case SH: return "SH";
	case SL: return "SL";
	case SQ: return "SQ";
	case SS: return "SS";
	case ST: return "ST";
	case TM: return "TM";
	case UI: return "UI";
	case UL: return "UL";
	case US: return "US";
	case UN: return "UN";
	case UT: return "UT";
	case OX: return "??";
	case NO: return "--";
	}
	return "zz";	// to satisfy compiler
}

void dump(char *filename)
{
	struct zzfile szz, *zz;
	uint16_t group, element;
	long len;
	const struct part6 *tag;
	char value[MAX_LEN_VALUE];
	char tmp[MAX_LEN_VALUE];
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

		if (zz->part6 && header == 0)
		{
			printf("\n# Dicom-Meta-Information-Header\n");
			printf("# Used TransferSyntax: %s\n", zz->ladder[1].txsyn == ZZ_EXPLICIT ? "Little Endian Explicit" : "Little Endian Implicit");
			header = 1;
		}
		else if (zz->current.group > 0x0002 && header < 2)
		{
			printf("\n# Dicom-Data-Set\n");
			printf("# Used TransferSyntax: %s\n", zz->ladder[0].txsyn == ZZ_EXPLICIT ? "Little Endian Explicit" : "Little Endian Implicit");
			header = 2;
		}

		memset(value, 0, sizeof(value));		// need to zero all first
		strcpy(value, "(unknown value format)");	// for implicit and no dictionary entry

		for (i = 0; i < zz->currNesting; i++) printf("  ");
		if (group > 0x0002 && element == 0x0000)	// generic group length
		{
			snprintf(value, sizeof(value) - 1, "[%u]", zzgetuint32(zz));
			printf("(%04x,%04x) UL %-42s # %4ld, 1 Generic Group Length\n", group, element, value, len);
			continue;
		}
		else if (group % 2 > 0 && element < 0x1000 && len != UNLIMITED)
		{
			zzgetstring(zz, tmp, sizeof(tmp));
			snprintf(value, sizeof(value) - 1, "[%s]", tmp);
			printf("(%04x,%04x) LO %-42s # %4ld, 1 Private Creator\n", group, element, value, len);
			continue;
		}
		else if (tag && zz->current.vr == NO && group != 0xfffe)
		{
			vr = ZZ_VR(tag->VR[0], tag->VR[1]);
		}

		if (len == 0)
		{
			strcpy(value, "(no value available)");
		}
		else if (group == 0xfffe)
		{
			memset(value, 0, sizeof(value));	// looks prettier empty
		}
		else if (vr == UL)
		{
			snprintf(value, sizeof(value) - 1, "[%u]", zzgetuint32(zz));
		}
		else if (vr == US)
		{
			snprintf(value, sizeof(value) - 1, "[%u]", zzgetuint16(zz));
		}
		else if (vr == SS)
		{
			snprintf(value, sizeof(value) - 1, "[%u]", zzgetint16(zz));
		}
		else if (vr == SL)
		{
			snprintf(value, sizeof(value) - 1, "[%u]", zzgetint32(zz));
		}
		else if (vr == FD)
		{
			snprintf(value, sizeof(value) - 1, "[%g]", zzgetdouble(zz));
		}
		else if (vr == FL)
		{
			snprintf(value, sizeof(value) - 1, "[%f]", zzgetfloat(zz));
		}
		else if ((vr == UN && len == UNLIMITED) || vr == SQ)
		{
			if (zz->ladder[zz->ladderidx].txsyn == ZZ_EXPLICIT || len == UNLIMITED)
			{
				strcpy(value, "(Sequence)");
			}
			else
			{
				strcpy(value, "(Sequence in limited UN - not parsed)");
			}
		}
		else if (vr == LO || vr == SH || vr == CS || vr == DS || vr == AE || vr == PN || vr == UI
		         || vr == LT || vr == AS || vr == DT || vr == IS || vr == UT || vr == TM || vr == DA)
		{
			zzgetstring(zz, tmp, sizeof(tmp));
			tmp[sizeof(tmp) - 4] = '\0';  // add trailing dots if cut off
			tmp[sizeof(tmp) - 5] = '.';
			tmp[sizeof(tmp) - 6] = '.';
			tmp[sizeof(tmp) - 7] = '.';
			snprintf(value, sizeof(value) - 1, "[%s]", tmp);
		}

		// Presenting in DCMTK's syntax
		printf("(%04x,%04x) %s %-42s # %4ld, %s %s\n", group, element, vr2str(vr), value, len, tag ? tag->VM : "?", tag ? tag->description : "?");
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

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
	long len, pos;
	const struct part6 *tag;
	char value[MAX_LEN_VALUE];
	char tmp[MAX_LEN_VALUE];
	int i;
	bool header = false;	// written header?

	zz = zzopen(filename, "r", &szz);

	if (zz)
	{
		printf("\n# Dicom-File-Format\n");
		if (zz->ladderidx > 0)
		{
			printf("\n# Dicom-Meta-Information-Header\n");
			printf("# Used TransferSyntax: %s\n", zz->ladder[1].txsyn == ZZ_EXPLICIT ? "Little Endian Explicit" : "Little Endian Implicit");
		}
	}

	while (zz && !feof(zz->fp) && !ferror(zz->fp))
	{
		zzread(zz, &group, &element, &len);

		pos = ftell(zz->fp);
		tag = zztag(group, element);

		if (zz->ladderidx == 0 && !header)
		{
			printf("\n# Dicom-Data-Set\n");
			printf("# Used TransferSyntax: %s\n", zz->ladder[0].txsyn == ZZ_EXPLICIT ? "Little Endian Explicit" : "Little Endian Implicit");
			header = true;
		}

		for (i = 0; i < zz->currNesting; i++) printf("  ");
		if (!tag)
		{
			printf("(%04x,%04x) %s -- unknown tag\n", group, element, zz->current.vr != NO ? vr2str(zz->current.vr) : "");
			if (!feof(zz->fp) && len != 0xFFFFFFFF && len > 0)
			{
				fseek(zz->fp, len, SEEK_CUR);
			}
			continue;
		}
		memset(value, 0, sizeof(value));
		if (len == 0)
		{
			strcpy(value, "(no value available)");
		}
		else if (tag->VR[0] == 'U' && tag->VR[1] == 'L')
		{
			snprintf(value, sizeof(value) - 1, "[%u]", zzgetuint32(zz));
		}
		else if (tag->VR[0] == 'U' && tag->VR[1] == 'S')
		{
			snprintf(value, sizeof(value) - 1, "[%u]", zzgetuint16(zz));
		}
		else if (tag->VR[0] == 'S' && tag->VR[1] == 'S')
		{
			snprintf(value, sizeof(value) - 1, "[%u]", zzgetint16(zz));
		}
		else if (tag->VR[0] == 'S' && tag->VR[1] == 'L')
		{
			snprintf(value, sizeof(value) - 1, "[%u]", zzgetint32(zz));
		}
		else if ((tag->VR[0] == 'L' && tag->VR[1] == 'O') || (tag->VR[0] == 'S' && tag->VR[1] == 'H')
		         || (tag->VR[0] == 'C' && tag->VR[1] == 'S') || (tag->VR[0] == 'D' && tag->VR[1] == 'S')
		         || (tag->VR[0] == 'A' && tag->VR[1] == 'E') || (tag->VR[0] == 'P' && tag->VR[1] == 'N')
		         || (tag->VR[0] == 'U' && tag->VR[1] == 'I') || (tag->VR[0] == 'L' && tag->VR[1] == 'T')
		         || (tag->VR[0] == 'A' && tag->VR[1] == 'S') || (tag->VR[0] == 'D' && tag->VR[1] == 'T')
		         || (tag->VR[0] == 'I' && tag->VR[1] == 'S') || (tag->VR[0] == 'U' && tag->VR[1] == 'T')
		         || (tag->VR[0] == 'T' && tag->VR[1] == 'M') || (tag->VR[0] == 'D' && tag->VR[1] == 'A'))
		{
			zzgetstring(zz, tmp, sizeof(tmp));
			tmp[sizeof(tmp) - 4] = '\0';  // add trailing dots if cut off
			tmp[sizeof(tmp) - 5] = '.';
			tmp[sizeof(tmp) - 6] = '.';
			tmp[sizeof(tmp) - 7] = '.';
			snprintf(value, sizeof(value) - 1, "[%s]", tmp);
		}

		// Presenting in DCMTK's syntax
		printf("(%04x,%04x) %s %-42s # %4ld, %s %s\n", tag->group, tag->element, zz->current.vr != NO ? vr2str(zz->current.vr) : tag->VR, 
		       value, len, tag->VM, tag->description);

		// Abort early, skip loading pixel data into memory if possible
		if (pos + len >= zz->fileSize)
		{
			break;
		}

		// Skip ahead
		if (!feof(zz->fp) && len != UNLIMITED && len > 0 && !(group == 0xfffe && element == 0xe000 && zz->pxstate != ZZ_PIXELITEM) 
		    && zz->current.vr != SQ)
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

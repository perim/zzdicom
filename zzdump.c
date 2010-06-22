#include "zz_priv.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define MAX_LEN_VALUE 40

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
	uint32_t len, pos;
	const struct part6 *tag;
	char value[MAX_LEN_VALUE];
	int i;

	zz = zzopen(filename, "r", &szz);

	while (zz && !feof(zz->fp) && !ferror(zz->fp))
	{
		zzread(zz, &group, &element, &len);

		pos = ftell(zz->fp);
		tag = zztag(group, element);

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
			strcpy(value, "(null)");
		}
		else if (tag->VR[0] == 'U' && tag->VR[1] == 'L')
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
		printf("(%04x,%04x) %s %-42s # %4d, %s %s\n", tag->group, tag->element, zz->current.vr != NO ? vr2str(zz->current.vr) : tag->VR, 
		       value, len, tag->VM, tag->description);

		// Abort early, skip loading pixel data into memory if possible
		if ((uint32_t)ftell(zz->fp) + len == zz->fileSize)
		{
			break;
		}

		// Skip ahead
		if (!feof(zz->fp) && len != 0xFFFFFFFF && len > 0 && !(group == 0xfffe && element == 0xe000) && zz->current.vr != SQ)
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

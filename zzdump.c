#include "zz_priv.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "part6.h"

#define MAX_LEN_VALUE 120
#define PADLEN 54

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

// Dump CSA1 or CSA2 private format
void dumpcsa(struct zzfile *zz)
{
	int i, k;

	char val[4];
	if (zz->current.length >= 4 && zisetreadpos(zz->zi, zz->current.pos) && ziread(zz->zi, &val, 4))
	{
		uint32_t ntags, unused32, pos;
		uint8_t unused8;
		char str[65], vr[5], c;
		uint32_t csavm, syngodt, nitems, xx, j, itemsize[4];

		for (i = 0; i < zz->currNesting + 1; i++) printf("  ");
		if (val[0] == 'S' && val[1] == 'V' && val[2] == '1' && val[3] == '0')
		{
			printf("\033[1m\033[31m^^ SIEMENS CSA DATA v2:\033[0m\n");
			ziread(zz->zi, &val, 4);
			if (val[0] != '\4' || val[1] != '\3' || val[2] != '\2' || val[3] != '\1')
			{
				printf("Bad CSA2 signature\n");
				return;
			}
			ziread(zz->zi, &ntags, 4);
		}
		else
		{
			printf("\033[1m\033[31m^^ SIEMENS CSA DATA v1:\033[0m\n");
			zisetreadpos(zz->zi, zz->current.pos);
			ziread(zz->zi, &ntags, 4);
		}
		ziread(zz->zi, &unused32, 4);
		if (unused32 != 77)
		{
			printf("Bad magic header value! (was %u)\n", unused32);
			return;
		}
		memset(str, 0, sizeof(str));
		memset(vr, 0, sizeof(vr));
		for (i = 0; i < (int)ntags; i++)
		{
			ziread(zz->zi, str, 64);
			ziread(zz->zi, &csavm, 4);
			ziread(zz->zi, vr, 4);
			ziread(zz->zi, &syngodt, 4);
			ziread(zz->zi, &nitems, 4);
			ziread(zz->zi, &xx, 4);
			if (xx != 77 && xx != 205)
			{
				printf("Bad magic tag value! (was %u)\n", xx);
				return;
			}
			for (j = 0; j < (unsigned)zz->currNesting + 2; j++) printf("  "); // indent
			printf("%d : items=%d, (%s) %s\n", i, nitems, vr, str);    // ugly for now
			memset(str, 0, sizeof(str));
			memset(vr, 0, sizeof(vr));
			for (j = 0; j < nitems; j++)
			{
				ziread(zz->zi, itemsize, 4 * 4);
				if (itemsize[2] != 77 && itemsize[2] != 205) // someone in Siemens must have a number fetish...
				{
					printf("Bad magic item value! (was %u) [%u,%u,%u,%u]\n", itemsize[2], itemsize[0], itemsize[1], itemsize[2], itemsize[3]); 
					return;
				}
				pos = zireadpos(zz->zi) + itemsize[1];
				if (pos - zireadpos(zz->zi) > zz->current.length)
				{
					printf("Ran out of buffer while parsing!\n");
					return;
				}
				zisetreadpos(zz->zi, pos);
				ziread(zz->zi, itemsize, (4 - itemsize[1] % 4) % 4); // discard padded garbage
			}
		}
	}
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
	char tmp[MAX_LEN_VALUE], vrstr[MAX_LEN_VR];
	int i, charlen;
	int header = 0;		// 0 - started, 1 - writing header, 2 - written header
	char extra[10], pstart[48], pstop[100];
	bool content;

	zz = zzopen(filename, "r", &szz);
	if (!zz)
	{
		exit(1);
	}

	zziterinit(zz);
	printf("\n# Dicom-File-Format\n");
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
		else if (zz->prividx >= 0)
		{
			privtag = zzprivtag(group, element, zz->privgroup[zz->prividx].creator, zz->privgroup[zz->prividx].offset);
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

		memset(pstart, 0, sizeof(pstart));
		memset(pstop, 0, sizeof(pstop));
		content = zztostring(zz, value, sizeof(value) - 2, PADLEN - 2);
		charlen = (strcmp(zz->characterSet, "ISO_IR 192") == 0) ? strlen_utf8(value) : strlen(value);
		if (content)
		{
			strcpy(pstart, "[");
			snprintf(pstop, sizeof(pstop) - 1, "]%*s", (int)(PADLEN - charlen - 2), "");
		}
		else
		{
			strcpy(pstart, "\033[22m\033[33m");
			snprintf(pstop, sizeof(pstop) - 1, "\033[0m%*s", (int)(PADLEN - charlen), "");
		}

		if (group > 0x0002 && element == 0x0000)	// generic group length
		{
			zz->current.vr = UL;
			description = "Generic Group Length";
			vm = "1";
		}
		else if (group % 2 > 0 && element < 0x1000 && element > 0x0010 && len != UNLIMITED)
		{
			if (zz->current.vr == NO) zz->current.vr = LO; // educated guess
			description = "Private Creator";
			vm = "1";
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
		printf("\033[22m\033[32m(%04x,%04x)\033[0m %s %s%s%s # %4s, %s %s%s\n",
		       group, element, zzvr2str(zz->current.vr, vrstr), pstart, value, pstop, tmp, vm, description, extra);

		if (!zz->current.valid)
		{
			for (i = 0; i < zz->currNesting; i++) printf("  ");
			printf("\033[1m\033[31m^^ Warning: %s\033[0m\n", zz->current.warning);
		}

		if (zz->current.group == 0x0029 && (zz->current.element >> 8) == 0x0010 && zz->prividx >= 0
		    && zz->current.vr == OB && strcmp(zz->privgroup[zz->prividx].creator, "SIEMENS CSA HEADER") == 0)
		{
			dumpcsa(zz);
		}
	}
	zz = zzclose(zz);
}

int main(int argc, char **argv)
{
	int i;

	for (i = zzutil(argc, argv, 2, "<filenames>", "DICOM tag dumper", NULL); i < argc; i++)
	{
		dump(argv[i]);
	}

	return 0;
}

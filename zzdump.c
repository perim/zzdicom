#include "zz_priv.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "part6.h"

#define MAX_LEN_VALUE 120
#define PADLEN 54

enum
{
	OPT_STDIN,
	OPT_FAKEDELIM,
	OPT_COUNT
};

static struct zzopts opts[] =
	{ { "--stdin", "Read data from standard input", true, false, 0, 0 }, // OPT_STDIN
	  { "--fakedelim", "Generate fake delimiters for items and sequences", false, false, 0, 0 }, // OPT_FAKEDELIM
	  { NULL, NULL, false, false, 0, 0 } };              // OPT_COUNT

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

bool checkCSA(struct zzfile *zz, const char **str, const char **vm)
{
	static char val[4];
	uint32_t ntags = 0;
	if (zz->current.length >= 4 && zisetreadpos(zz->zi, zz->current.pos) && ziread(zz->zi, &val, 4))
	{
		if (val[0] == 'S' && val[1] == 'V' && val[2] == '1' && val[3] == '0')
		{
			ziread(zz->zi, &val, 4);
			if (val[0] != '\4' || val[1] != '\3' || val[2] != '\2' || val[3] != '\1')
			{
				return false; // Bad CSA2 signature
			}
			ziread(zz->zi, &ntags, 4);
			snprintf(val, 4, "%u", ntags);
			*vm = val;
			*str = "SIEMENS CSA DATA v2";
			return true;
		}
		else if (val[0] != 00 || val[1] != 0)
		{
			ziread(zz->zi, &ntags, 4);
			snprintf(val, 4, "%u", ntags);
			*vm = val;
			*str = "SIEMENS CSA DATA v1";
			return true;
		}
		else if ((val[0] == '\0' || val[0] == '\6') && val[1] == '\0' && val[2] != '\0')
		{
			// neither CSA1 nor CSA2 nor INTERFILE, some older(?) format based on DICOM
			*str = "SIEMENS CSA DATASET";
			return true;
		}
	}
	return false;
}

char *getstring(struct zzfile *zz, char *input, long strsize, long datasize)
{
	const long desired = MIN(datasize, strsize - 1);
	int result, last;
	char temp[MAX_LEN_VALUE];
	memset(input, 0, strsize);
	memset(temp, 0, sizeof(temp));
	result = last = ziread(zz->zi, temp, desired);
	memcpy(input, temp, desired);
	input[result] = '\0'; // make sure we zero terminate
	while (last-- > 0 && (input[last] == ' ' || input[last] == '\0'
	       || input[last] == '\t' || input[last] == '\n')) // remove trailing whitespace
	{
		if (input[last] == '\n')
			input[last] = '\\';
		else
			input[last] = '\0';
	}
	return (result == desired) ? input : NULL;
}

// Dump CSA1 or CSA2 private format
void dumpcsa(struct zzfile *zz)
{
	char val[4];
	if (zz->current.length >= 4 && zisetreadpos(zz->zi, zz->current.pos) && ziread(zz->zi, &val, 4))
	{
		uint32_t ntags, unused32, pos;
		unsigned charlen, sum;
		char str[65], vr[5];
		uint32_t csavm, syngodt, nitems, xx, j, itemsize[4];
		char value[PADLEN];
		char pstart[48], pstop[100];
		int i;

		memset(value, 0, sizeof(value));
		if (val[0] == 'S' && val[1] == 'V' && val[2] == '1' && val[3] == '0')
		{
			ziread(zz->zi, &val, 4);
			if (val[0] != '\4' || val[1] != '\3' || val[2] != '\2' || val[3] != '\1')
			{
				return; // give up for now
			}
		}
		else if (val[0] != 00 || val[1] != 0)
		{
			zisetreadpos(zz->zi, zz->current.pos);
		}
		else // format loosely based on DICOM, fake a sequence of it
		{
			zisetreadpos(zz->zi, zz->current.pos);
			zz->current.vr = SQ;
			zz->ladderidx++;
			zz->ladder[zz->ladderidx].group = zz->current.group;
			zz->ladder[zz->ladderidx].element = zz->current.element;
			zz->ladder[zz->ladderidx].size = UNLIMITED;
			zz->ladder[zz->ladderidx].type = ZZ_SEQUENCE;
			zz->ladder[zz->ladderidx + 0].item = -1;
			zz->ladder[zz->ladderidx + 1].item = -1;
			zz->ladder[zz->ladderidx].txsyn = ZZ_EXPLICIT;
			zz->ladder[zz->ladderidx].pos = zz->current.pos;
			zz->nextNesting += 1;
			zz->current.length = UNLIMITED; // force parsing
			zz->current.group = 0x000; // silence verify warning
			zz->current.element = 0x000; // silence verify warning
			zz->ladder[zz->ladderidx].csahack = true; // ignore groups etc
			return;
		}
		ziread(zz->zi, &ntags, 4);
		ziread(zz->zi, &unused32, 4);
		if (unused32 != 77)
		{
			debug("Bad magic header value! (was %u)", unused32);
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
				debug("Bad magic tag value! (was %u)", xx);
				return;
			}
			for (j = 0; j < (unsigned)zz->currNesting + 1; j++) printf("  "); // indent

			sum = 0;
			charlen = 0;
			memset(value, 0, sizeof(value));
			for (j = 0; j < nitems; j++)
			{
				ziread(zz->zi, itemsize, 4 * 4);
				if (itemsize[2] != 77 && itemsize[2] != 205) // someone in Siemens must have a number fetish...
				{
					debug("Bad magic item value! (was %u) [%u,%u,%u,%u]", itemsize[2], itemsize[0], itemsize[1], itemsize[2], itemsize[3]); 
					return;
				}
				sum += itemsize[1];
				if (zz->current.pos - zireadpos(zz->zi) + (long)itemsize[1] > zz->current.length)
				{
					debug("Ran out of buffer while parsing!");
					return;
				}
				pos = zireadpos(zz->zi) + itemsize[1];
				if (charlen < sizeof(value) - 1)
				{
					if (j > 0 && itemsize[1] > 0 && j < nitems - 1 && charlen < sizeof(value) - 2)
					{
						value[charlen + 0] = '\\';
						value[charlen + 1] = '\0';
						charlen++;
					}
					getstring(zz, value + charlen, sizeof(value) - charlen - 1, itemsize[1]);
					charlen = strlen(value);
					if (charlen >= sizeof(value) - 3)
					{
						value[sizeof(value) - 2] = '\0';
						value[sizeof(value) - 3] = '.';
						value[sizeof(value) - 4] = '.';
						value[sizeof(value) - 5] = '.';
					}
				}
				zisetreadpos(zz->zi, pos);
				ziread(zz->zi, itemsize, (4 - itemsize[1] % 4) % 4); // discard padded garbage
			}

			// Presenting in pseudo-DCMTK syntax
			if (sum > 0)
			{
				charlen = strlen(value);
				strcpy(pstart, "[");
				snprintf(pstop, sizeof(pstop) - 1, "]%*s", (int)(PADLEN - charlen - 2), "");
			}
			else
			{
				strcpy(value, "no value available");
				charlen = strlen(value);
				strcpy(pstart, "\033[22m\033[33m(");
				snprintf(pstop, sizeof(pstop) - 1, ")\033[0m%*s", (int)(PADLEN - charlen - 2), "");
			}
			printf("\033[22m\033[32m(----,----)\033[0m %s %s%s%s # %4d, %d %s\n",
			       vr, pstart, value, pstop, sum, nitems, str);
			memset(str, 0, sizeof(str));
			memset(vr, 0, sizeof(vr));
		}
	}
}

void dump(struct zzfile *zz)
{
	uint16_t group, element;
	long len;
	const char *vm, *description, *csa = NULL;
	const struct part6 *tag;
	const struct privatedic *privtag;
	char value[MAX_LEN_VALUE];
	char tmp[MAX_LEN_VALUE], vrstr[MAX_LEN_VR];
	int i, charlen;
	int header = 0;		// 0 - started, 1 - writing header, 2 - written header
	char extra[64], pstart[48], pstop[100];
	bool content;

	zziterinit(zz);
	printf("\n# Dicom-File-Format\n");

	while (zziternext(zz, &group, &element, &len))
	{
		if (zz->current.fake)
		{
			if (opts[OPT_FAKEDELIM].found)
			{
				for (i = 0; i < zz->currNesting; i++) printf("  ");
				printf("(%04x, %04x) --\n", group, element);
			}
			continue;
		}
		// Extra checks
		if (!zz->ladder[zz->ladderidx].csahack)
		{
			zzverify(zz);
		}

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

		if (group > 0x0002 && element == 0x0000 && !zz->ladder[zz->ladderidx].csahack)	// generic group length
		{
			if (zz->current.vr == NO) zz->current.vr = UL; // has to be
			description = "Generic Group Length";
			vm = "1";
		}
		else if (group % 2 > 0 && element < 0x1000 && element > 0x0010 && len != UNLIMITED && !zz->ladder[zz->ladderidx].csahack)
		{
			if (zz->current.vr == NO) zz->current.vr = LO; // educated guess
			description = "Private Creator";
			vm = "1";
		}

		memset(pstart, 0, sizeof(pstart));
		memset(pstop, 0, sizeof(pstop));
		content = zztostring(zz, value, sizeof(value) - 2, PADLEN - 2);
		charlen = (strcmp(zz->characterSet, "ISO_IR 192") == 0) ? strlen_utf8(value) : strlen(value);
		csa = NULL;
		if (content)
		{
			strcpy(pstart, "[");
			snprintf(pstop, sizeof(pstop) - 1, "]%*s", (int)(PADLEN - charlen - 2), "");
		}
		else if (zz->current.group == 0x0029
		         && ((zz->current.element & 0xff) == 0x0010 || (zz->current.element & 0xff) == 0x0020)
		         && zz->prividx >= 0
		         && zz->current.vr == OB && strcmp(zz->privgroup[zz->prividx].creator, "SIEMENS CSA HEADER") == 0
		         && checkCSA(zz, &csa, &vm))
		{
			strcpy(value, csa);
			charlen = strlen(csa);
			strcpy(pstart, "\033[22m\033[33m(");
			snprintf(pstop, sizeof(pstop) - 1, ")\033[0m%*s", (int)(PADLEN - charlen - 2), "");
		}
		else
		{
			strcpy(pstart, "\033[22m\033[33m");
			snprintf(pstop, sizeof(pstop) - 1, "\033[0m%*s", (int)(PADLEN - charlen), "");
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

		if (csa)
		{
			dumpcsa(zz);
		}
	}
}

int main(int argc, char **argv)
{
	int i;

	i = zzutil(argc, argv, 1, "[<filenames>]", "DICOM tag dumper", opts);
	if (opts[OPT_STDIN].found)
	{
		struct zzfile szz, *zz;
		zz = zzstdin(&szz);
		dump(zz);
		zzclose(zz);
		return 0;
	}
	for (; i < argc; i++)
	{
		const char *filename = argv[i];
		struct zzfile szz, *zz;
		zz = zzopen(filename, "r", &szz);
		if (!zz)
		{
			exit(1);
		}
		dump(zz);
		zz = zzclose(zz);
	}

	return 0;
}

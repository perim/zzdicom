#include "byteorder.h"

#ifdef POSIX
#define _XOPEN_SOURCE 600
#include <fcntl.h>
#endif

#include "zz.h"
#include "zz_priv.h"

#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>

const char *versionString =
#include "VERSION"
;

static bool verbose = false;
static bool testOnly = false;

struct zzfile *zzopen(const char *filename, const char *mode, struct zzfile *infile)
{
	char transferSyntaxUid[MAX_LEN_UID];
	struct zzfile *zz;
	char dicm[4], endianbuf[6];
	uint16_t group, element;
	uint32_t len, cur;
	bool done = false;
	struct stat st;

	if (!infile || !mode || !filename)
	{
		return NULL;
	}
	zz = infile;
	memset(zz, 0, sizeof(*zz));
	if (stat(filename, &st) != 0)
	{
		fprintf(stderr, "%s could not be found: %s\n", filename, strerror(errno));
		return NULL;
	}
	zz->fp = fopen(filename, mode);
	if (!zz->fp)
	{
		fprintf(stderr, "%s could not be opened: %s\n", filename, strerror(errno));
		return NULL;
	}
	zz->fileSize = st.st_size;
	zz->modifiedTime = st.st_mtime;
	realpath(filename, zz->fullPath);
#ifdef POSIX
	posix_fadvise(fileno(zz->fp), 0, 4096 * 4, POSIX_FADV_SEQUENTIAL);	// request 4 pages right away
#endif

	// Check for valid Part 10 header
	if (fseek(zz->fp, 128, SEEK_SET) != 0 || fread(dicm, 4, 1, zz->fp) != 1 || strncmp(dicm, "DICM", 4) != 0)
	{
		fprintf(stderr, "%s does not have a valid part 10 DICOM header\n", filename);
		rewind(zz->fp);	// try anyway
	}

	// Check for big-endian syntax - not supported
	if (fread(&endianbuf, 1, 6, zz->fp) != 6 || fseek(zz->fp, -6, SEEK_CUR) != 0 || endianbuf[0] < endianbuf[1])
	{
		fprintf(stderr, "%s appears to be big-endian - this is not supported\n", filename);
		return zzclose(zz);
	}

	// Naive check for explicit, but unlikely to fail since size of the first tag would have to be very
	// large to have two uppercase letters in it, and it is always very small. Reusing excess data in endianbuf.
	// The reason for this check is that some broken early DICOM implementations wrote the header in implicit.
	if (isupper(endianbuf[4]) && isupper(endianbuf[5]))
	{
		zz->ladder[0].txsyn = ZZ_EXPLICIT;
	}
	else
	{
		zz->ladder[0].txsyn = ZZ_IMPLICIT;
	}

	// Grab some useful data before handing back control
	zz->ladder[0].pos = ftell(zz->fp);
	while (zzread(zz, &group, &element, &len) && !done && !feof(zz->fp) && !ferror(zz->fp))
	{
		int result = len;

		cur = ftell(zz->fp);
		switch (ZZ_KEY(group, element))
		{
		case DCM_FileMetaInformationGroupLength:
			zz->ladderidx = 1;
			zz->ladder[1].pos = ftell(zz->fp);
			zz->ladder[1].size = zzgetuint32(zz);
			zz->ladder[1].txsyn = zz->ladder[0].txsyn;
			zz->ladder[1].group = 0x0002;
			break;
		case DCM_MediaStorageSOPClassUID:
			result = fread(zz->sopClassUid, 1, MIN(sizeof(zz->sopClassUid) - 1, len), zz->fp);
			break;
		case DCM_MediaStorageSOPInstanceUID:
			result = fread(zz->sopInstanceUid, 1, MIN(sizeof(zz->sopInstanceUid) - 1, len), zz->fp);
			break;
		case DCM_TransferSyntaxUID:
			result = fread(transferSyntaxUid, 1, MIN(sizeof(transferSyntaxUid) - 1, len), zz->fp);
			done = true;	// not ACR-NEMA, last interesting tag, so stop scanning
			zz->acrNema = false;
			if (strcmp(transferSyntaxUid, UID_LittleEndianImplicitTransferSyntax) == 0)
			{
				// once over the header, start parsing implicit
				zz->ladder[0].txsyn = ZZ_IMPLICIT;
			}
			else if (strcmp(transferSyntaxUid, UID_BigEndianExplicitTransferSyntax) == 0)
			{
				fprintf(stderr, "%s - big endian transfer syntax found - not supported", filename);
				fclose(zz->fp);
				return NULL;
			}
			// else continue to believe it is explicit little-endian, which really is the only sane thing to use
			break;
		case DCM_ACR_NEMA_RecognitionCode:
			done = true;
			zz->acrNema = true;
			break;
		default:
			break;
		}
		if (result != (int)len)
		{
			fprintf(stderr, "%s failed to read data value (read %d, wanted %d)\n", filename, result, (int)len);
			return zzclose(zz);
		}
		if (!feof(zz->fp) && !ferror(zz->fp))
		{
			fseek(zz->fp, cur + len, SEEK_SET);	// skip data
		}
	}
	fseek(zz->fp, zz->ladder[0].pos, SEEK_SET);

	return zz;
}

uint32_t zzgetuint32(struct zzfile *zz)
{
	uint32_t val;

	fread(&val, 4, 1, zz->fp);
	return LE_32(val);
}

uint16_t zzgetuint16(struct zzfile *zz)
{
	uint16_t val;

	fread(&val, 2, 1, zz->fp);
	return LE_16(val);
}

int32_t zzgetint32(struct zzfile *zz)
{
	int32_t val;

	fread(&val, 4, 1, zz->fp);
	return LE_32(val);
}

int16_t zzgetint16(struct zzfile *zz)
{
	int16_t val;

	fread(&val, 2, 1, zz->fp);
	return LE_16(val);
}

bool zzread(struct zzfile *zz, uint16_t *group, uint16_t *element, uint32_t *len)
{
	enum zztxsyn syntax = zz->ladder[zz->ladderidx].txsyn;
	const uint32_t ladderpos = zz->ladder[zz->ladderidx].pos;
	const uint32_t laddersize = zz->ladder[zz->ladderidx].size;
	// Represent the three different variants of tag headers in one union
	struct
	{
		uint16_t group;
		uint16_t element;
		union { uint32_t len; struct { char vr[2]; uint16_t len; } evr; } buffer;
	} header;
	zzKey key;

	fread(&header, 8, 1, zz->fp);		// group+element then either VR + 0, VR+VL, or just VL
	*group = header.group;
	*element = header.element;
	zz->currNesting = zz->nextNesting;
	key = ZZ_KEY(header.group, header.element);

	// Did we leave a group, sequence or item?
	if (zz->ladderidx > 0 && ((laddersize != 0xffff && (uint32_t)ftell(zz->fp) - ladderpos > laddersize) || key == DCM_SequenceDelimitationItem || key == DCM_ItemDelimitationItem))
	{
		if (zz->ladder[zz->ladderidx].group == 0xffff)	// leaving a sequence or item, ie not a group
		{
			zz->currNesting--;
			zz->nextNesting--;
		}
		zz->ladderidx--;
		syntax = zz->ladder[zz->ladderidx].txsyn;
	}

	if (key == DCM_Item)
	{
		zz->nextNesting++;
	}

	// Try explicit VR
	if (syntax == ZZ_EXPLICIT && key != DCM_Item && key != DCM_ItemDelimitationItem && key != DCM_SequenceDelimitationItem)
	{
		zz->current.vr = ZZ_VR(header.buffer.evr.vr[0], header.buffer.evr.vr[1]);

		switch (zz->current.vr)
		{
		case SQ:
		case UN:
			zz->nextNesting++;
			// fall through
		case OB:
		case OW:
		case OF:
		case UT:
			fread(len, 4, 1, zz->fp);		// the 32 bit variant
			*len = LE_32(*len);
			break;
		default:
			*len = LE_16(header.buffer.evr.len);	// the insane 16 bit size variant
			break;
		}
	}
	else	// the sad legacy implicit variant
	{
		zz->current.vr = NO;	// no info
		*len = LE_32(header.buffer.len);
	}

	if (header.element == 0x0000 || (key != DCM_PixelData && *len == UNLIMITED) || zz->current.vr == SQ || key == DCM_Item)
	{
		// Entered into a group or sequence, copy parameters
		if (zz->ladderidx >= MAX_LADDER)
		{
			return false;	// stop parsing and give up!
		}
		zz->ladderidx++;
		zz->ladder[zz->ladderidx].pos = ftell(zz->fp);
		zz->ladder[zz->ladderidx].size = *len;
		if (zz->current.vr != UN)
		{
			zz->ladder[zz->ladderidx].txsyn = zz->ladder[zz->ladderidx - 1].txsyn;	// inherit transfer syntax
		}
		else
		{
			zz->ladder[zz->ladderidx].txsyn = ZZ_IMPLICIT;	// UN is always implicit
		}
		zz->ladder[zz->ladderidx].group = header.element == 0x0000 ? header.group : 0xffff;	// mark as group or sequence
	}

	return true;
}

static const struct part6 part6_table[] = 
{
#include "part6.c"
};

const struct part6 *zztag(uint16_t group, uint16_t element)
{
	// Since we read DICOM files sequentially by ascending group/element tags, assuming the same for 
	// tag lookups much improves lookup speed. Hence the lastidx hack.
	static int lastidx = 0;
	const int max = ARRAY_SIZE(part6_table);
	int i;

	for (i = lastidx; i < max; i++)
	{
		if (group == part6_table[i].group && element == part6_table[i].element)
		{
			lastidx = i;
			return &part6_table[i];
		}
	}
	for (i = 0; i < lastidx; i++)
	{
		if (group == part6_table[i].group && element == part6_table[i].element)
		{
			lastidx = i;
			return &part6_table[i];
		}
	}
	return NULL;
}

int zzutil(int argc, char **argv, int minArgs, const char *usage, const char *help)
{
	FILE *out = stderr;
	int i, ignparams = 1;	// always tell caller to ignore argv[0]
	bool usageReq = false;

	for (i = 1; i < argc; i++)
	{
		if (strcmp(argv[i], "--") == 0)
		{
			ignparams++;
			break;	// stop parsing command line parameters
		}
		else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
		{
			fprintf(stdout, "%s\n", help);
			fprintf(stdout, "  %-10s Verbose output\n", "-v");
			//fprintf(stdout, "  %-10s Test only - apply no changes\n", "-t");
			fprintf(stdout, "  %-10s This help\n", "-h|--help");
			fprintf(stdout, "  %-10s Version of zzdicom package\n", "--version");
			fprintf(stdout, "  %-10s Short help\n", "--usage");
			usageReq = true;
			out = stdout;
		}
		else if (strcmp(argv[i], "--usage") == 0)
		{
			usageReq = true;
			out = stdout;
		}
		else if (strcmp(argv[i], "--version") == 0)
		{
			fprintf(stdout, "%s is part of zzdicom version %s\n", argv[0], versionString);
			exit(0);
		}
		else if (strcmp(argv[i], "-v") == 0)	// verbose
		{
			ignparams++;
			verbose = true;
		}
		else if (strcmp(argv[i], "-t") == 0)	// test only
		{
			ignparams++;
			testOnly = true;
		}
	}
	if (usageReq || argc < minArgs + ignparams - 1)
	{
		fprintf(out, "Usage: %s [-v|--version|-h|--usage] %s\n", argv[0], usage); // add -t later
		exit(!usageReq);
	}
	return ignparams;
}

void zz_c_test()
{
	char filename[12];
	struct zzfile szz;
	int fd;
	char value;

	assert(zzopen(NULL, NULL, NULL) == NULL);
	assert(zzopen("/nonexistent", "r", &szz) == NULL);
	strcpy(filename, "/tmp/XXXXXX");
	fd = mkstemp(filename);
	fchmod(fd, 0);	// make inaccessible
	assert(zzopen(filename, "r", &szz) == NULL);
	fchmod(fd, S_IWUSR | S_IRUSR);	// make accessible
	value = 1; write(fd, &value, 1);
	value = 0; write(fd, &value, 1);
	assert(zzopen(filename, "r", &szz) == NULL);	// pretends to be big-endian
	close(fd);
}

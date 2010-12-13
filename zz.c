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

struct zzfile *zzopen(const char *filename, const char *mode, struct zzfile *infile)
{
	struct zzfile *zz;
	char dicm[4], endianbuf[6];
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
	if (!zz->fp || !realpath(filename, zz->fullPath))
	{
		fprintf(stderr, "%s could not be opened: %s\n", filename, strerror(errno));
		return NULL;
	}
	zz->fileSize = st.st_size;
	zz->modifiedTime = st.st_mtime;
#ifdef POSIX
	posix_fadvise(fileno(zz->fp), 0, 4096 * 4, POSIX_FADV_SEQUENTIAL);	// request 4 pages right away
#endif

	// Check for valid Part 10 header
	zz->part10 = true;	// ever the optimist
	if (fseek(zz->fp, 128, SEEK_SET) != 0 || fread(dicm, 4, 1, zz->fp) != 1 || strncmp(dicm, "DICM", 4) != 0)
	{
		fprintf(stderr, "%s does not have a valid part 10 DICOM header\n", filename);
		rewind(zz->fp);	// try anyway
		zz->part10 = false;
	}

	if (fread(&endianbuf, 1, 6, zz->fp) != 6 || fseek(zz->fp, -6, SEEK_CUR) != 0)
	{
		return NULL;	// not big enough to be a DICOM file
	}

	// Safety check - are we really reading a part 10 file? First tag MUST be (0x0002, 0x0000)
	if (endianbuf[0] != 2 || endianbuf[1] != 0 || endianbuf[2] != 0 || endianbuf[3] != 0)
	{
		rewind(zz->fp);				// rewind and try over without the part10
		fread(&endianbuf, 1, 6, zz->fp);
		fseek(zz->fp, -6, SEEK_CUR);
		zz->part10 = false;
	}

	// Check for big-endian syntax - not supported
	if (endianbuf[0] < endianbuf[1])
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

	zz->ladder[0].pos = ftell(zz->fp);

	return zz;
}

char *zztostring(struct zzfile *zz, char *input, long strsize)
{
	memset(input, 0, strsize);
	if (zz->current.length == 0)
	{
		strncpy(input, "(no value available)", strsize);
		input[strsize-1] = '\0';
		return input;
	}
	switch (zz->current.vr)
	{
	case UN:
	case SQ:
		if ((zz->current.vr == UN && zz->current.length == UNLIMITED) || zz->current.vr == SQ)
		{
			strncpy(input, "(Sequence)", strsize);
		}
		else
		{
			strncpy(input, "(Sequence in limited UN - not parsed)", strsize);
		}
		break;
	case OB: case OW: case OF: case UT:
		strcpy(input, "...");
		break;
	case AE: case AS: case AT: case CS: case DA: case DS: case DT: case IS:
	case LT: case PN: case SH: case ST: case TM: case UI: case LO:
		if (!zzgetstring(zz, input + 1, strsize - 1))
		{
			strncpy(input, "(Error)", strsize);
			break;
		}
		input[0] = '[';
		input[strsize-1] = '\0';
		if (zz->current.length > strsize - 4)
		{
			input[strsize - 2] = ']';
			input[strsize - 3] = '.';
			input[strsize - 4] = '.';
			input[strsize - 5] = '.';
		}
		else
		{
			input[strlen(input) + 0] = ']';
			input[strlen(input) + 1] = '\0';
		}
		break;
	case UL:
		snprintf(input, strsize - 1, "[%u]", zzgetuint32(zz, 0));
		break;
	case US:
		snprintf(input, strsize - 1, "[%u]", zzgetuint16(zz, 0));
		break;
	case SS:
		snprintf(input, strsize - 1, "[%d]", zzgetint16(zz, 0));
		break;
	case SL:
		snprintf(input, strsize - 1, "[%d]", zzgetint32(zz, 0));
		break;
	case FL:
		snprintf(input, strsize - 1, "[%f]", zzgetfloat(zz, 0));
		break;
	case FD:
		snprintf(input, strsize - 1, "[%g]", zzgetdouble(zz, 0));
		break;
	case OX:
	case NO:
		break;
	}
	input[strsize-1] = '\0';
	return input;
}

char *zzgetstring(struct zzfile *zz, char *input, long strsize)
{
	const long desired = MIN(zz->current.length, strsize - 1);
	long result, last;

	last = result = fread(input, 1, desired, zz->fp);
	input[MIN(result, strsize - 1)] = '\0';	// make sure we zero terminate
	while (last > 0 && input[--last] == ' ')	// remove trailing whitespace
	{
		input[last] = '\0';
	}
	return (result == desired) ? input : NULL;
}

#define CHECK_SEEK_READ(zz, val, idx) \
	zz->current.length >= (long)sizeof(val) * (idx + 1) && fseek(zz->fp, zz->current.pos + idx * sizeof(val), SEEK_SET) == 0 && fread(&val, sizeof(val), 1, zz->fp) == 1

float zzgetfloat(struct zzfile *zz, int idx)
{
	float val;

	if (CHECK_SEEK_READ(zz, val, idx))
	{
		return LE_32(val);
	}
	return 0.0f;
}

double zzgetdouble(struct zzfile *zz, int idx)
{
	double val;

	if (CHECK_SEEK_READ(zz, val, idx))
	{
		return LE_64(val);
	}
	return 0.0;
}

uint32_t zzgetuint32(struct zzfile *zz, int idx)
{
	uint32_t val;

	if (CHECK_SEEK_READ(zz, val, idx))
	{
		return LE_32(val);
	}
	return 0;
}

uint16_t zzgetuint16(struct zzfile *zz, int idx)
{
	uint16_t val;

	if (CHECK_SEEK_READ(zz, val, idx))
	{
		return LE_16(val);
	}
	return 0;
}

int32_t zzgetint32(struct zzfile *zz, int idx)
{
	int32_t val;

	if (CHECK_SEEK_READ(zz, val, idx))
	{
		return LE_32(val);
	}
	return 0;
}

int16_t zzgetint16(struct zzfile *zz, int idx)
{
	int16_t val;

	if (CHECK_SEEK_READ(zz, val, idx))
	{
		return LE_16(val);
	}
	return 0;
}

bool zzread(struct zzfile *zz, uint16_t *group, uint16_t *element, long *len)
{
	char transferSyntaxUid[MAX_LEN_UID];
	// Represent the three different variants of tag headers in one union
	struct
	{
		uint16_t group;
		uint16_t element;
		union { uint32_t len; struct { char vr[2]; uint16_t len; } evr; } buffer;
	} header;
	zzKey key;
	long pos;

	if (fread(&header, 8, 1, zz->fp) != 1)		// group+element then either VR + 0, VR+VL, or just VL
	{
		return false;
	}
	zz->current.group = *group = header.group;
	zz->current.element = *element = header.element;
	zz->currNesting = zz->nextNesting;
	key = ZZ_KEY(header.group, header.element);

	// Did we leave a group, sequence or item? We can drop out of multiple at the same time.
	while (zz->ladderidx > 0)
	{
		long bytesread = ftell(zz->fp) - zz->ladder[zz->ladderidx].pos;

		if (zz->ladder[zz->ladderidx].type == ZZ_GROUP
		    && (zz->current.group != zz->ladder[zz->ladderidx].group
		        || bytesread > zz->ladder[zz->ladderidx].size
		        || key == DCM_SequenceDelimitationItem
		        || key == DCM_ItemDelimitationItem))
		{
			zz->ladderidx--;	// end parsing this group now
			continue;
		}
		else if (zz->ladder[zz->ladderidx].type == ZZ_ITEM
		         && (bytesread > zz->ladder[zz->ladderidx].size
		             || key == DCM_SequenceDelimitationItem
		             || key == DCM_ItemDelimitationItem))
		{
			if (bytesread > zz->ladder[zz->ladderidx].size)
			{
				zz->currNesting--;
			}
			zz->nextNesting--;
			zz->ladderidx--;	// end parsing this item now
			continue;
		}
		else if (zz->ladder[zz->ladderidx].type == ZZ_SEQUENCE
		         && (bytesread > zz->ladder[zz->ladderidx].size
		             || key == DCM_SequenceDelimitationItem))
		{
			if (bytesread > zz->ladder[zz->ladderidx].size)
			{
				zz->currNesting--;
			}
			zz->nextNesting--;
			zz->ladderidx--;	// end parsing this sequence now
			key = 0;		// do not react twice on the same sequence delimiter
			continue;
		}
		break;		// no further cause for regress found
	}
	key = ZZ_KEY(header.group, header.element);	// restore key

	// Try explicit VR
	if (zz->ladder[zz->ladderidx].txsyn != ZZ_IMPLICIT && key != DCM_Item && key != DCM_ItemDelimitationItem && key != DCM_SequenceDelimitationItem)
	{
		uint32_t rlen;
		zz->current.vr = ZZ_VR(header.buffer.evr.vr[0], header.buffer.evr.vr[1]);

		switch (zz->current.vr)
		{
		case SQ:
		case UN:
		case OB:
		case OW:
		case OF:
		case UT:
			if (fread(&rlen, 4, 1, zz->fp) != 1)		// the 32 bit variant
			{
				return false;
			}
			*len = LE_32(rlen);
			break;
		default:
			*len = LE_16(header.buffer.evr.len);	// the insane 16 bit size variant
			break;
		}
		if (zz->current.vr == SQ || (*len == UNLIMITED && zz->current.vr == UN))
		{
			zz->nextNesting++;
		}
	}
	else	// the sad legacy implicit variant
	{
		zz->current.vr = NO;	// no info
		*len = LE_32(header.buffer.len);
		if (*len == UNLIMITED && key != DCM_PixelData && key != DCM_Item && key != DCM_SequenceDelimitationItem && key != DCM_ItemDelimitationItem)
		{
			zz->nextNesting++;
		}
	}
	zz->current.length = *len;
	pos = zz->current.pos = ftell(zz->fp);	// anything we read after this, we roll back the position for

	switch (key)
	{
	case DCM_Item:
		zz->nextNesting++;
		if (zz->pxstate == ZZ_PIXELDATA)
		{
			zz->pxstate = ZZ_OFFSET_TABLE;
		}
		else if (zz->pxstate == ZZ_OFFSET_TABLE)
		{
			zz->pxstate = ZZ_PIXELITEM;
		}
		break;
	case DCM_PixelData:
		if (*len == UNLIMITED)
		{
			// Start special ugly handling of the encapsulated pixel data attribute
			zz->pxstate = ZZ_PIXELDATA;
			zz->nextNesting++;
		}
		break;
	case DCM_SequenceDelimitationItem:
		if (zz->pxstate != ZZ_NOT_PIXEL)
		{
			zz->pxstate = ZZ_NOT_PIXEL;
			zz->currNesting--;
			zz->nextNesting--;
		}
		break;
	case DCM_MediaStorageSOPClassUID:
		zzgetstring(zz, zz->sopClassUid, sizeof(zz->sopClassUid) - 1);
		break;
	case DCM_MediaStorageSOPInstanceUID:
		zzgetstring(zz, zz->sopInstanceUid, sizeof(zz->sopInstanceUid) - 1);
		break;
	case DCM_TransferSyntaxUID:
		zzgetstring(zz, transferSyntaxUid, sizeof(transferSyntaxUid) - 1);
		if (strcmp(transferSyntaxUid, UID_LittleEndianImplicitTransferSyntax) == 0)
		{
			// once over the header, start parsing implicit
			zz->ladder[0].txsyn = ZZ_IMPLICIT;
		}
		else if (strcmp(transferSyntaxUid, UID_DeflatedExplicitVRLittleEndianTransferSyntax) == 0)
		{
			// once over the header, start inflating
			zz->ladder[0].txsyn = ZZ_EXPLICIT_COMPRESSED;
			fprintf(stderr, "Deflate transfer syntax found - not supported yet\n");
			return false;
		}
		else if (strcmp(transferSyntaxUid, UID_BigEndianExplicitTransferSyntax) == 0)
		{
			fprintf(stderr, "Big endian transfer syntax found - not supported\n");
			return false;
		}
		// else continue to believe it is explicit little-endian, which really is the only sane thing to use
		break;
	case DCM_ACR_NEMA_RecognitionCode:
		zz->acrNema = true;
		break;
	case DCM_ItemDelimitationItem:
	default:
		break;
	}

	if (key == DCM_FileMetaInformationGroupLength)
	{
		zz->ladderidx = 1;
		zz->ladder[1].pos = ftell(zz->fp);
		zz->ladder[1].size = zzgetuint32(zz, 0);
		zz->ladder[1].txsyn = zz->ladder[0].txsyn;
		zz->ladder[1].group = 0x0002;
		zz->ladder[1].type = ZZ_GROUP;
	}
	else if (header.element == 0x0000 || (key != DCM_PixelData && *len == UNLIMITED) || zz->current.vr == SQ || key == DCM_Item)
	{
		// Entered into a group or sequence, copy parameters
		if (zz->ladderidx >= MAX_LADDER)
		{
			fprintf(stderr, "Too deep group/sequence nesting - giving up\n");
			return false;	// stop parsing and give up!
		}
		zz->ladderidx++;
		if (header.element == 0x0000)
		{
			zz->ladder[zz->ladderidx].size = zzgetuint32(zz, 0);
			zz->ladder[zz->ladderidx].group = *group;
			zz->ladder[zz->ladderidx].type = ZZ_GROUP;
		}
		else
		{
			zz->ladder[zz->ladderidx].size = *len;
			zz->ladder[zz->ladderidx].group = 0xffff;
			zz->ladder[zz->ladderidx].type = (key == DCM_Item) ? ZZ_ITEM : ZZ_SEQUENCE;
		}
		zz->ladder[zz->ladderidx].pos = ftell(zz->fp);
		if (zz->current.vr != UN)
		{
			zz->ladder[zz->ladderidx].txsyn = zz->ladder[zz->ladderidx - 1].txsyn;	// inherit transfer syntax
		}
		else
		{
			zz->ladder[zz->ladderidx].txsyn = ZZ_IMPLICIT;	// UN is always implicit
		}
	}
	fseek(zz->fp, pos, SEEK_SET);	// rollback data reading to allow user app to read too

	return true;
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
	}
	if (usageReq || argc < minArgs + ignparams - 1)
	{
		fprintf(out, "Usage: %s [-v|--version|-h|--usage] %s\n", argv[0], usage); // add -t later
		exit(!usageReq);
	}
	return ignparams;
}

void zziterinit(struct zzfile *zz)
{
	if (zz)
	{
		fseek(zz->fp, zz->ladder[0].pos, SEEK_SET);
		zz->currNesting = 0;
		zz->nextNesting = 0;
		zz->ladderidx = 0;
		zz->current.group = 0;
		zz->current.element = 0;
		zz->current.length = 0;
		zz->current.pos = -1;
		zz->pxstate = ZZ_NOT_PIXEL;
	}
}

bool zziternext(struct zzfile *zz, uint16_t *group, uint16_t *element, long *len)
{
	// Check if we should read the next tag -- try to iterate over as many tags as possible, even if data is totally fubar
	if (zz && !feof(zz->fp) && !ferror(zz->fp)
	    && (zz->current.length == UNLIMITED || (zz->current.pos + zz->current.length < zz->fileSize) || zz->current.vr == SQ || zz->current.group == 0xfffe))
	{
		if (zz->current.pos > 0 && zz->current.length > 0 && zz->current.length != UNLIMITED
		    && !(zz->current.group == 0xfffe && zz->current.element == 0xe000 && zz->pxstate == ZZ_NOT_PIXEL)
		    && zz->current.vr != SQ)
		{
			fseek(zz->fp, zz->current.pos + zz->current.length, SEEK_SET);	// go to start of next tag
			// note if this conditional is not entered, we will try to parse the contents of the tag
		}
		if (zzread(zz, group, element, len))
		{
			return true;
		}
	}
	*len = 0;
	return false;	// do NOT use any other returned data in this case!
}

void zz_c_test()
{
	char filename[12];
	struct zzfile szz;
	int fd, i;
	char value;

	assert(zzopen(NULL, NULL, NULL) == NULL);
	assert(zzopen("/nonexistent", "r", &szz) == NULL);
	strcpy(filename, "/tmp/XXXXXX");
	fd = mkstemp(filename);
	fchmod(fd, 0);	// make inaccessible
	assert(zzopen(filename, "r", &szz) == NULL);
	fchmod(fd, S_IWUSR | S_IRUSR);	// make accessible
	value = 0; write(fd, &value, 1);
	value = 1; write(fd, &value, 1);
	assert(zzopen(filename, "r", &szz) == NULL);	// too small
	for (i = 0; i < 128; i++) write(fd, &value, 1);	// filler
	assert(zzopen(filename, "r", &szz) == NULL);	// pretends to be big-endian
	close(fd);
}

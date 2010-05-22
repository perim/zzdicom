#include "byteorder.h"

#ifdef POSIX
#define _XOPEN_SOURCE 600
#include <fcntl.h>
#endif

#include "zz.h"
#include "zz_priv.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

struct zzfile *zzopen(const char *filename, const char *mode)
{
	struct zzfile *zz;
	char dicm[4], endianbuf[2];
	uint16_t group, element;
	uint32_t len, cur;
	bool done = false;

	zz = malloc(sizeof(*zz));
	memset(zz, 0, sizeof(*zz));
	zz->fp = fopen(filename, mode);
	if (!zz->fp)
	{
		fprintf(stderr, "%s not found\n", filename);
		free(zz);
		return NULL;
	}
	zz->fullPath = realpath(filename, NULL);
#ifdef POSIX
	posix_fadvise(fileno(zz->fp), 0, 4096 * 4, POSIX_FADV_SEQUENTIAL);	// request 4 pages right away
#endif

	// Check for valid Part 10 header
	fseek(zz->fp, 128, SEEK_SET);
	fread(dicm, 4, 1, zz->fp);
	if (strncmp(dicm, "DICM", 4) != 0)
	{
		fprintf(stderr, "%s does not have a valid part 10 DICOM header\n", filename);
		rewind(zz->fp);
	}

	// Check for big-endian syntax - not supported
	fread(&endianbuf, 1, 2, zz->fp);
	fseek(zz->fp, -2, SEEK_CUR);
	if (endianbuf[0] < endianbuf[1])
	{
		fprintf(stderr, "%s appears to be big-endian - this is not supported\n", filename);
		fclose(zz->fp);
		free(zz);
		return NULL;
	}

	// Grab some useful data before handing back control
	zz->startPos = ftell(zz->fp);
	while (zzread(zz, &group, &element, &len) && !done)
	{
		cur = ftell(zz->fp);
		switch (ZZ_KEY(group, element))
		{
		case DCM_FileMetaInformationGroupLength:
			zz->headerSize = zzgetuint32(zz);
			break;
		case DCM_MediaStorageSOPClassUID:
			fread(zz->sopClassUid, MIN(sizeof(zz->sopClassUid) - 1, len), 1, zz->fp);
			break;
		case DCM_MediaStorageSOPInstanceUID:
			fread(zz->sopInstanceUid, MIN(sizeof(zz->sopInstanceUid) - 1, len), 1, zz->fp);
			break;
		case DCM_TransferSyntaxUID:
			fread(zz->sopInstanceUid, MIN(sizeof(zz->transferSyntaxUid) - 1, len), 1, zz->fp);
			done = true;	// not ACR-NEMA, last interesting tag, so stop scanning
			zz->acrNema = false;
			break;
		case DCM_ACR_NEMA_RecognitionCode:
			done = true;
			zz->acrNema = true;
			break;
		default:
			break;
		}
		fseek(zz->fp, cur + len, SEEK_SET);	// skip data
	}
	fseek(zz->fp, zz->startPos, SEEK_SET);

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
	// Represent the three different variants of tag headers in one union
	union { uint32_t len; struct { char vr[2]; uint16_t len; } evr; } buffer;

	*group = zzgetuint16(zz);
	*element = zzgetuint16(zz);
	fread(&buffer, 4, 1, zz->fp);		// either VR + 0, VR+VL, or just VL

	// Try explicit VR
	if (isupper(buffer.evr.vr[0]) && isupper(buffer.evr.vr[1]))
	{
		const char *vr = buffer.evr.vr;

		// Try explicit VR of type OB, OW, OF, SQ, UT or UN
		if ((vr[0] == 'O' && (vr[1] == 'B' || vr[1] == 'W' || vr[1] == 'F'))
		    || (vr[0] == 'S' && vr[1] == 'Q') || (vr[0] == 'U' && (vr[1] == 'T' || vr[1] == 'N')))
		{
			fread(len, 4, 1, zz->fp);
			*len = LE_32(*len);
		}
		else	// TODO check VR types
		{
			*len = LE_16(buffer.evr.len);	// the insane 16 bit size variant
		}
	}
	else
	{
		*len = LE_32(buffer.len);
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

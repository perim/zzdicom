#ifdef POSIX
#define _XOPEN_SOURCE 600
#include <fcntl.h>
#endif

#include "zz_priv.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

FILE *zzopen(const char *filename, const char *mode)
{
	FILE *fp;
	char dicm[4], endianbuf[2];

	fp = fopen(filename, mode);
	if (!fp)
	{
		fprintf(stderr, "%s not found\n", filename);
		exit(-1);
	}
#ifdef POSIX
	posix_fadvise(fileno(fp), 0, 4096 * 4, POSIX_FADV_SEQUENTIAL);	// request 4 pages right away
#endif

	// Check for valid Part 10 header
	fseek(fp, 128, SEEK_SET);
	fread(dicm, 4, 1, fp);
	if (strncmp(dicm, "DICM", 4) != 0)
	{
		fprintf(stderr, "%s does not have a valid part 10 DICOM header\n", filename);
		rewind(fp);
	}

	// Check for big-endian syntax - not supported
	fread(&endianbuf, 1, 2, fp);
	fseek(fp, -2, SEEK_CUR);
	if (endianbuf[0] < endianbuf[1])
	{
		fprintf(stderr, "%s appears to be big-endian - this is not supported\n", filename);
		exit(-1);
	}
	return fp;
}

uint32_t zzgetuint32(FILE *fp)
{
	uint32_t val;

	fread(&val, 4, 1, fp);
	return val;
}

uint16_t zzgetuint16(FILE *fp)
{
	uint16_t val;

	fread(&val, 2, 1, fp);
	return val;
}

int32_t zzgetint32(FILE *fp)
{
	int32_t val;

	fread(&val, 4, 1, fp);
	return val;
}

int16_t zzgetint16(FILE *fp)
{
	int16_t val;

	fread(&val, 2, 1, fp);
	return val;
}

bool zzread(FILE *fp, uint16_t *group, uint16_t *element, uint32_t *len)
{
	char *vr;
	uint16_t buffer[2], buffer2[2];

	*group = zzgetuint16(fp);
	*element = zzgetuint16(fp);
	fread(buffer, 2, 2, fp);		// either VR + 0, VR+VL, or just VL

	// Try explicit VR
	vr = (char *)buffer;
	if (isupper(vr[0]) && isupper(vr[1]))
	{
		// Try explicit VR of type OB, OW, OF, SQ, UT or UN
		if ((vr[0] == 'O' && (vr[1] == 'B' || vr[1] == 'W' || vr[1] == 'F'))
		    || (vr[0] == 'S' && vr[1] == 'Q') || (vr[0] == 'U' && (vr[1] == 'T' || vr[1] == 'N')))
		{
			fread(buffer2, 2, 2, fp);
			*len = ((uint32_t)buffer2[0]) + ((uint32_t)buffer2[1] << 16);
		}
		else	// TODO check VR types
		{
			*len = buffer[1];
		}
	}
	else
	{
		*len = ((uint32_t)buffer[0]) + ((uint32_t)buffer[1] << 16);
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

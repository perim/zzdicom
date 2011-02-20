#include <string.h>
#include <errno.h>

#include "zz_priv.h"
#include "byteorder.h"
#include "zzwrite.h"

static inline bool explicit(struct zzfile *zz) { return zz->ladder[zz->ladderidx].txsyn == ZZ_EXPLICIT; }

static bool implicit(FILE *fp, uint16_t group, uint16_t element, uint32_t length)
{
	bool ret;
	ret = fwrite(&group, 2, 1, fp) == 1;
	ret = ret && fwrite(&element, 2, 1, fp) == 1;
	ret = ret && fwrite(&length, 4, 1, fp) == 1;
	return ret;
}

static bool explicit1(FILE *fp, uint16_t group, uint16_t element, const char *vr, uint16_t length)
{
	bool ret;
	ret = fwrite(&group, 2, 1, fp) == 1;
	ret = ret && fwrite(&element, 2, 1, fp) == 1;
	ret = ret && fwrite(&vr[0], 1, 1, fp) == 1;
	ret = ret && fwrite(&vr[1], 1, 1, fp) == 1;
	ret = ret && fwrite(&length, 2, 1, fp) == 1;
	return ret;
}

static bool explicit2(FILE *fp, uint16_t group, uint16_t element, const char *vr, uint32_t length)
{
	uint16_t zero = 0;
	bool ret;

	ret = fwrite(&group, 2, 1, fp) == 1;
	ret = ret && fwrite(&element, 2, 1, fp) == 1;
	ret = ret && fwrite(&vr[0], 1, 1, fp) == 1;
	ret = ret && fwrite(&vr[1], 1, 1, fp) == 1;
	ret = ret && fwrite(&zero, 2, 1, fp) == 1;
	ret = ret && fwrite(&length, 4, 1, fp) == 1;
	return ret;
}

static inline bool writetag(struct zzfile *zz, zzKey key, enum VR vr, uint32_t size)
{
	const uint16_t group = ZZ_GROUP(key);
	const uint16_t element = ZZ_ELEMENT(key);
	char dest[MAX_LEN_VR];

	if (!zz->ladder[zz->ladderidx].txsyn == ZZ_EXPLICIT)
	{
		return implicit(zz->fp, group, element, size);
	}
	else
	{
		switch (vr)
		{
		case OB: case OW: case OF: case SQ: case UT: case UN:
			return explicit2(zz->fp, group, element, zzvr2str(vr, dest), size);
		default:
			return explicit1(zz->fp, group, element, zzvr2str(vr, dest), size);
		}
	}
}

bool zzwSQ(struct zzfile *zz, zzKey key, uint32_t size)
{
	return writetag(zz, key, SQ, size);
}

bool zzwUN(struct zzfile *zz, zzKey key, uint32_t size)
{
	return writetag(zz, key, UN, size);
}

void zzwUN_begin(struct zzfile *zz, zzKey key, long *pos)
{
	zzwUN(zz, key, UNLIMITED);
	if (pos) *pos = ftell(zz->fp) - 4;	// position of length value
	zz->ladderidx++;
	zz->ladder[zz->ladderidx].txsyn = ZZ_IMPLICIT;
	zz->ladder[zz->ladderidx].size = UNLIMITED;
	zz->ladder[zz->ladderidx].group = ZZ_GROUP(key);
	zz->ladder[zz->ladderidx].type = ZZ_SEQUENCE;
}

/// Pass in NULL to use UNLIMITED size
void zzwUN_end(struct zzfile *zz, long *pos)
{
	if (pos)	// set exact size of data written
	{
		long curr = ftell(zz->fp);
		uint32_t len = curr - *pos - 4;

		fseek(zz->fp, *pos, SEEK_SET);
		fwrite(&len, 4, 1, zz->fp);
		fseek(zz->fp, curr, SEEK_SET);
	}
	else
	{
		implicit(zz->fp, 0xfffe, 0xe0dd, 0);	// add end seq tag
	}
	zz->ladderidx--;
}

void zzwItem_begin(struct zzfile *zz, long *pos)
{
	implicit(zz->fp, 0xfffe, 0xe000, UNLIMITED);
	if (pos) *pos = ftell(zz->fp) - 4;	// position of length value
}

void zzwItem_end(struct zzfile *zz, long *pos)
{
	if (pos)	// set exact size of data written
	{
		long curr = ftell(zz->fp);
		uint32_t len = curr - *pos - 4;

		fseek(zz->fp, *pos, SEEK_SET);
		fwrite(&len, 4, 1, zz->fp);
		fseek(zz->fp, curr, SEEK_SET);
	}
	else
	{
		implicit(zz->fp, 0xfffe, 0xe00d, 0);	// add end item tag
	}
}

void zzwSQ_begin(struct zzfile *zz, zzKey key, long *pos)
{
	zzwSQ(zz, key, UNLIMITED);
	if (pos) *pos = ftell(zz->fp) - 4;	// position of length value
	zz->ladderidx++;
	zz->ladder[zz->ladderidx].txsyn = ZZ_EXPLICIT;
	zz->ladder[zz->ladderidx].size = UNLIMITED;
	zz->ladder[zz->ladderidx].group = ZZ_GROUP(key);
	zz->ladder[zz->ladderidx].type = ZZ_SEQUENCE;
}

void zzwSQ_end(struct zzfile *zz, long *pos)
{
	zzwUN_end(zz, pos);
}

void zzwPixelData_begin(struct zzfile *zz, long frames, enum VR vr)
{
	int i;
	uint32_t tmp = 0;

	writetag(zz, DCM_PixelData, vr, UNLIMITED);
	implicit(zz->fp, 0xfffe, 0xe000, sizeof(tmp) * frames);
	zz->pxOffsetTable = ftell(zz->fp);	// position of index table
	for (i = 0; i < frames; i++)
	{
		fwrite(&tmp, sizeof(tmp), 1, zz->fp);	// write empty index table
	}
	zz->frames = frames;
}

void zzwPixelData_frame(struct zzfile *zz, int frame, const char *data, uint32_t size)
{
	long curr = ftell(zz->fp);
	uint32_t offset = curr - (zz->pxOffsetTable + zz->frames * sizeof(offset));
	long pos = zz->pxOffsetTable + frame * sizeof(offset);

	fseek(zz->fp, pos, SEEK_SET);
	fwrite(&offset, 4, 1, zz->fp);
	fseek(zz->fp, curr, SEEK_SET);
	implicit(zz->fp, 0xfffe, 0xe000, size);
	fwrite(data, size, 1, zz->fp);
	// TODO pad to even length?
}

void zzwPixelData_end(struct zzfile *zz)
{
	implicit(zz->fp, 0xfffe, 0xe0dd, 0);
}

bool zzwAT(struct zzfile *zz, zzKey key, zzKey key2)
{
	const uint16_t group2 = ZZ_GROUP(key2);
	const uint16_t element2 = ZZ_ELEMENT(key2);
	bool ret;

	ret = writetag(zz, key, AT, sizeof(group2) + sizeof(element2));
	ret = ret && fwrite(&group2, sizeof(group2), 1, zz->fp) == 1;
	ret = ret && fwrite(&element2, sizeof(element2), 1, zz->fp) == 1;
	return ret;
}

bool zzwUL(struct zzfile *zz, zzKey key, uint32_t value)
{
	bool ret = writetag(zz, key, UL, sizeof(value));
	ret = ret && fwrite(&value, sizeof(value), 1, zz->fp) == 1;
	return ret;
}

bool zzwULv(struct zzfile *zz, zzKey key, int len, const uint32_t *value)
{
	bool ret = writetag(zz, key, UL, sizeof(*value) * len);
	ret = ret && fwrite(value, sizeof(*value), len, zz->fp) == (size_t)len;
	return ret;
}

bool zzwSL(struct zzfile *zz, zzKey key, int32_t value)
{
	bool ret = writetag(zz, key, SL, sizeof(value));
	ret = ret && fwrite(&value, sizeof(value), 1, zz->fp) == 1;
	return ret;
}

bool zzwSS(struct zzfile *zz, zzKey key, int16_t value)
{
	bool ret = writetag(zz, key, SS, sizeof(value));
	ret = ret && fwrite(&value, sizeof(value), 1, zz->fp) == 1;
	return ret;
}

bool zzwUS(struct zzfile *zz, zzKey key, uint16_t value)
{
	bool ret = writetag(zz, key, US, sizeof(value));
	ret = ret && fwrite(&value, sizeof(value), 1, zz->fp) == 1;
	return ret;
}

bool zzwFL(struct zzfile *zz, zzKey key, float value)
{
	bool ret = writetag(zz, key, FL, sizeof(value));
	ret = ret && fwrite(&value, sizeof(value), 1, zz->fp) == 1;
	return ret;
}

bool zzwFD(struct zzfile *zz, zzKey key, double value)
{
	bool ret = writetag(zz, key, FD, sizeof(value));
	ret = ret && fwrite(&value, sizeof(value), 1, zz->fp) == 1;
	return ret;
}

bool zzwOB(struct zzfile *zz, zzKey key, int len, const char *string)
{
	int wlen = len;
	bool ret;

	if (len % 2 != 0) wlen++;			// padding
	ret = writetag(zz, key, OB, wlen);
	ret = ret && fwrite(string, 1, len, zz->fp) == 1;
	if (len % 2 != 0) ret = ret && fwrite("", 1, 1, zz->fp) == 1;	// pad
	return ret;
}

bool zzwOW(struct zzfile *zz, zzKey key, int len, const uint16_t *string)
{
	bool ret = writetag(zz, key, OW, len * 2);
	ret = ret && fwrite(string, 2, len, zz->fp) == (size_t)len;
	return ret;
}

bool zzwOF(struct zzfile *zz, zzKey key, int len, const float *string)
{
	bool ret = writetag(zz, key, OF, len * 4);
	ret = ret && fwrite(string, 4, len, zz->fp) == (size_t)len;
	return ret;
}

bool zzwUI(struct zzfile *zz, zzKey key, const char *string)
{
	int length = MIN(strlen(string), (size_t)64);
	int wlen = length;
	bool ret;

	if (length % 2 != 0) wlen++;			// padding
	ret = writetag(zz, key, UI, wlen);
	ret = ret && fwrite(string, 1, length, zz->fp) == (size_t)length;
	if (length % 2 != 0) ret = ret && fwrite("", 1, 1, zz->fp) == 1;	// pad with null
	return ret;
}

static inline int countdelims(const char *str, const char delim)
{
	unsigned i, c = 0;
	for (i = 0; i < strlen(str); i++)
	{
		if (str[i] == delim) c++;
	}
	return c;
}

static bool wstr(struct zzfile *zz, zzKey key, const char *string, enum VR vr, size_t maxlen)
{
	const size_t multiplen = maxlen * (countdelims(string, '\\') + 1);	// max length times value multiplicity
	const size_t length = MIN(strlen(string), multiplen);
	size_t wlen = length;
	bool ret;

	if (length % 2 != 0) wlen++;			// padding
	ret = writetag(zz, key, vr, wlen);
	ret = ret && (fwrite(string, 1, length, zz->fp) == length);
	if (length % 2 != 0)
	{
		// pad with a space to make even length
		ret = ret && fwrite(" ", 1, 1, zz->fp) == 1;
	}
	return ret;
}
bool zzwPN(struct zzfile *zz, zzKey key, const char *string) { return wstr(zz, key, string, PN, 64); }
bool zzwSH(struct zzfile *zz, zzKey key, const char *string) { return wstr(zz, key, string, SH, 16); }
bool zzwAE(struct zzfile *zz, zzKey key, const char *string) { return wstr(zz, key, string, AE, 16); }
bool zzwAS(struct zzfile *zz, zzKey key, const char *string) { return wstr(zz, key, string, AS, 4); }
bool zzwCS(struct zzfile *zz, zzKey key, const char *string) { return wstr(zz, key, string, CS, 16); }
bool zzwLO(struct zzfile *zz, zzKey key, const char *string) { return wstr(zz, key, string, LO, 64); }
bool zzwLT(struct zzfile *zz, zzKey key, const char *string) { return wstr(zz, key, string, LT, 10240); }
bool zzwST(struct zzfile *zz, zzKey key, const char *string) { return wstr(zz, key, string, ST, 1024); }
bool zzwUT(struct zzfile *zz, zzKey key, const char *string) { return wstr(zz, key, string, UT, UINT32_MAX - 1); }
bool zzwDSs(struct zzfile *zz, zzKey key, const char *string) { return wstr(zz, key, string, DS, 16); }

bool zzwIS(struct zzfile *zz, zzKey key, int value)
{
	char tmp[MAX_LEN_IS];

	memset(tmp, 0, sizeof(tmp));
	snprintf(tmp, sizeof(tmp) - 1, "%d", value);
	return wstr(zz, key, tmp, IS, 12);
}

bool zzwDSd(struct zzfile *zz, zzKey key, double value)
{
	char tmp[MAX_LEN_DS];

	memset(tmp, 0, sizeof(tmp));
	snprintf(tmp, sizeof(tmp) - 1, "%g", value);
	return wstr(zz, key, tmp, DS, 16);
}

bool zzwDSdv(struct zzfile *zz, zzKey key, int len, const double *value)
{
	size_t wlen = 0, lenval;
	bool ret = true;
	long pos = ftell(zz->fp), now;
	int i;
	char tmp[MAX_LEN_DS + 1];

	// Set to position of size field, depending on transfer syntax
	pos += explicit(zz) ? 6 : 4;
	writetag(zz, key, DS, 0);

	// Write out values
	for (i = 0; i < len; i++)
	{
		memset(tmp, 0, sizeof(tmp));
		snprintf(tmp, sizeof(tmp) - 2, "%g", value[i]);
		lenval = strlen(tmp);
		if (i + 1 < len)
		{
			strcat(tmp, "\\");	// value delimiter
			lenval++;
		}
		ret = ret && fwrite(tmp, 1, lenval, zz->fp) == lenval;
		wlen += lenval;
	}
	if (wlen % 2 != 0)
	{
		wlen++;						// padding
		ret = ret && fwrite(" ", 1, 1, zz->fp) == 1;	// pad with a space to make even length
	}

	// Now set the size of the tag
	now = ftell(zz->fp);
	fseek(zz->fp, pos, SEEK_SET);
	if (explicit(zz))
	{
		uint16_t val = wlen;
		ret = ret && fwrite(&val, 2, 1, zz->fp) == 1;
	}
	else
	{
		uint32_t val = wlen;
		ret = ret && fwrite(&val, 4, 1, zz->fp) == 1;
	}
	fseek(zz->fp, now, SEEK_SET);
	return ret;
}

bool zzwDA(struct zzfile *zz, zzKey key, time_t datestamp)
{
	char tmp[MAX_LEN_DA];
	struct tm stamp;

	localtime_r(&datestamp, &stamp);
	strftime(tmp, MAX_LEN_DA, "%Y%m%d", &stamp);
	return wstr(zz, key, tmp, DA, 8);
}

bool zzwTM(struct zzfile *zz, zzKey key, struct timeval datetimestamp)
{
	char tmp[MAX_LEN_TM], frac[8];
	struct tm stamp;
	time_t datestamp = datetimestamp.tv_sec;

	localtime_r(&datestamp, &stamp);
	strftime(tmp, MAX_LEN_DT, "%H%M%S", &stamp);
	if (datetimestamp.tv_usec > 0)
	{
		memset(frac, 0, sizeof(frac));
		snprintf(frac, 7, ".%06u", (unsigned)datetimestamp.tv_usec);
		strcat(tmp, frac);
	}
	return wstr(zz, key, tmp, TM, 16);
}

bool zzwDT(struct zzfile *zz, zzKey key, struct timeval datetimestamp)
{
	char tmp[MAX_LEN_DT], frac[8];
	struct tm stamp;
	time_t datestamp = datetimestamp.tv_sec;

	localtime_r(&datestamp, &stamp);
	strftime(tmp, MAX_LEN_DT, "%Y%m%d%H%M%S", &stamp);
	if (datetimestamp.tv_usec > 0)
	{
		memset(frac, 0, sizeof(frac));
		snprintf(frac, 7, ".%06u", (unsigned)datetimestamp.tv_usec);
		strcat(tmp, frac);
	}
	return wstr(zz, key, tmp, DT, 26);
}

struct zzfile *zzcreate(const char *filename, struct zzfile *zz, const char *sopclass, const char *sopinstanceuid, const char *transfer)
{
	memset(zz, 0, sizeof(*zz));
	zz->acrNema = false;
	zz->ladder[0].txsyn = ZZ_EXPLICIT;
	zz->fp = fopen(filename, "w");
	if (!zz->fp) return NULL;
	zzwHeader(zz, sopclass, sopinstanceuid, transfer);
	if (strcmp(transfer, UID_LittleEndianImplicitTransferSyntax) == 0)
	{
		zz->ladder[0].txsyn = ZZ_IMPLICIT;
	}
	return zz;
}

void zzwHeader(struct zzfile *zz, const char *sopclass, const char *sopinstanceuid, const char *transfer)
{
	char version[3];
	const char *dicm = "DICM";
	char zeroes[128];			// FIXME, i hate this inefficiency
	uint32_t size;
	const uint32_t startpos = 128 + 4 + 8;

	version[0] = 0;
	version[1] = 1;
	version[2] = 0;
	memset(zeroes, 0, sizeof(zeroes));

	fwrite(zeroes, 1, 128, zz->fp);
	fwrite(dicm, 1, 4, zz->fp);

	zzwUL(zz, DCM_FileMetaInformationGroupLength, 0);	// length zero, fixing it below
	zzwOB(zz, DCM_FileMetaInformationVersion, 2, version);
	zzwUI(zz, DCM_MediaStorageSOPClassUID, sopclass);
	zzwUI(zz, DCM_MediaStorageSOPInstanceUID, sopinstanceuid);
	zzwUI(zz, DCM_TransferSyntaxUID, transfer);
	zzwUI(zz, DCM_ImplementationClassUID, "1.2.3.4.8.2");
	zzwSH(zz, DCM_ImplementationVersionName, "zzdicom");
	zzwAE(zz, DCM_SourceApplicationEntityTitle, "ZZ_NONE");

	// write group size
	size = ftell(zz->fp) - (startpos + 4);
	fseek(zz->fp, startpos, SEEK_SET);
	fwrite(&size, 4, 1, zz->fp);		// set size
	fseek(zz->fp, 0, SEEK_END);		// return to position
}

bool zzwEmpty(struct zzfile *zz, zzKey key, enum VR vr)
{
	return writetag(zz, key, vr, 0);
}

#include <string.h>
#include <errno.h>

#include "zz_priv.h"
#include "byteorder.h"
#include "zzwrite.h"

static inline bool explicit(struct zzfile *zz) { return zz->ladder[zz->ladderidx].txsyn == ZZ_EXPLICIT; }

static void implicit(FILE *fp, uint16_t group, uint16_t element, uint32_t length)
{
	fwrite(&group, 2, 1, fp);
	fwrite(&element, 2, 1, fp);
	fwrite(&length, 4, 1, fp);
}

static void explicit1(FILE *fp, uint16_t group, uint16_t element, const char *vr, uint16_t length)
{
	fwrite(&group, 2, 1, fp);
	fwrite(&element, 2, 1, fp);
	fwrite(&vr[0], 1, 1, fp);
	fwrite(&vr[1], 1, 1, fp);
	fwrite(&length, 2, 1, fp);
}

static void explicit2(FILE *fp, uint16_t group, uint16_t element, const char *vr, uint32_t length)
{
	uint16_t zero = 0;

	fwrite(&group, 2, 1, fp);
	fwrite(&element, 2, 1, fp);
	fwrite(&vr[0], 1, 1, fp);
	fwrite(&vr[1], 1, 1, fp);
	fwrite(&zero, 2, 1, fp);
	fwrite(&length, 4, 1, fp);
}

void zzwSQ(struct zzfile *zz, zzKey key, uint32_t size)
{
	const uint16_t group = ZZ_GROUP(key);
	const uint16_t element = ZZ_ELEMENT(key);

	if (!explicit(zz)) implicit(zz->fp, group, element, size);
	else explicit2(zz->fp, group, element, "SQ", size);
}

void zzwUN(struct zzfile *zz, zzKey key, uint32_t size)
{
	const uint16_t group = ZZ_GROUP(key);
	const uint16_t element = ZZ_ELEMENT(key);

	if (!explicit(zz)) implicit(zz->fp, group, element, size);
	else explicit2(zz->fp, group, element, "UN", size);
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
		uint32_t len = curr - *pos;

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

void zzwUL(struct zzfile *zz, zzKey key, uint32_t value)
{
	const uint16_t group = ZZ_GROUP(key);
	const uint16_t element = ZZ_ELEMENT(key);

	if (explicit(zz)) explicit1(zz->fp, group, element, "UL", sizeof(value));
	else implicit(zz->fp, group, element, sizeof(value));
	fwrite(&value, sizeof(value), 1, zz->fp);
}

void zzwULa(struct zzfile *zz, zzKey key, const uint32_t *value, int elems)
{
	const uint16_t group = ZZ_GROUP(key);
	const uint16_t element = ZZ_ELEMENT(key);

	if (explicit(zz)) explicit1(zz->fp, group, element, "UL", sizeof(*value) * elems);
	else implicit(zz->fp, group, element, sizeof(*value) * elems);
	fwrite(value, sizeof(*value), elems, zz->fp);
}

void zzwSL(struct zzfile *zz, zzKey key, int32_t value)
{
	const uint16_t group = ZZ_GROUP(key);
	const uint16_t element = ZZ_ELEMENT(key);

	if (explicit(zz)) explicit1(zz->fp, group, element, "SL", sizeof(value));
	else implicit(zz->fp, group, element, sizeof(value));
	fwrite(&value, sizeof(value), 1, zz->fp);
}

void zzwSS(struct zzfile *zz, zzKey key, int16_t value)
{
	const uint16_t group = ZZ_GROUP(key);
	const uint16_t element = ZZ_ELEMENT(key);

	if (explicit(zz)) explicit1(zz->fp, group, element, "SS", sizeof(value));
	else implicit(zz->fp, group, element, sizeof(value));
	fwrite(&value, sizeof(value), 1, zz->fp);
}

void zzwUS(struct zzfile *zz, zzKey key, uint16_t value)
{
	const uint16_t group = ZZ_GROUP(key);
	const uint16_t element = ZZ_ELEMENT(key);

	if (explicit(zz)) explicit1(zz->fp, group, element, "US", sizeof(value));
	else implicit(zz->fp, group, element, sizeof(value));
	fwrite(&value, sizeof(value), 1, zz->fp);
}

void zzwFL(struct zzfile *zz, zzKey key, float value)
{
	const uint16_t group = ZZ_GROUP(key);
	const uint16_t element = ZZ_ELEMENT(key);

	if (explicit(zz)) explicit1(zz->fp, group, element, "FL", sizeof(value));
	else implicit(zz->fp, group, element, sizeof(value));
	fwrite(&value, sizeof(value), 1, zz->fp);
}

void zzwFD(struct zzfile *zz, zzKey key, double value)
{
	const uint16_t group = ZZ_GROUP(key);
	const uint16_t element = ZZ_ELEMENT(key);

	if (explicit(zz)) explicit1(zz->fp, group, element, "FD", sizeof(value));
	else implicit(zz->fp, group, element, sizeof(value));
	fwrite(&value, sizeof(value), 1, zz->fp);
}

void zzwOB(struct zzfile *zz, zzKey key, const char *string, int length)
{
	const uint16_t group = ZZ_GROUP(key);
	const uint16_t element = ZZ_ELEMENT(key);
	int wlen = length;

	if (length % 2 != 0) wlen++;			// padding
	if (explicit(zz)) explicit2(zz->fp, group, element, "OB", wlen);
	else implicit(zz->fp, group, element, wlen);
	fwrite(string, 1, length, zz->fp);
	if (length % 2 != 0) fwrite("", 1, 1, zz->fp);	// pad
}

void zzwOW(struct zzfile *zz, zzKey key, const char *string, int length)
{
	const uint16_t group = ZZ_GROUP(key);
	const uint16_t element = ZZ_ELEMENT(key);

	if (explicit(zz)) explicit2(zz->fp, group, element, "OW", length * 2);
	else implicit(zz->fp, group, element, length * 2);
	fwrite(string, 2, length, zz->fp);
}

void zzwUI(struct zzfile *zz, zzKey key, const char *string)
{
	const uint16_t group = ZZ_GROUP(key);
	const uint16_t element = ZZ_ELEMENT(key);
	int length = MIN(strlen(string), (size_t)64);
	int wlen = length;

	if (length % 2 != 0) wlen++;			// padding
	if (explicit(zz)) explicit1(zz->fp, group, element, "UI", wlen);
	else implicit(zz->fp, group, element, wlen);
	fwrite(string, 1, length, zz->fp);
	if (length % 2 != 0) fwrite("", 1, 1, zz->fp);	// pad with null
}

static void wstr(struct zzfile *zz, zzKey key, const char *string, const char *vr, size_t maxlen)
{
	const uint16_t group = ZZ_GROUP(key);
	const uint16_t element = ZZ_ELEMENT(key);
	int length = MIN(strlen(string), maxlen);
	int wlen = length;

	if (length % 2 != 0) wlen++;			// padding
	if (explicit(zz)) explicit1(zz->fp, group, element, vr, wlen);
	else implicit(zz->fp, group, element, wlen);
	fwrite(string, 1, length, zz->fp);
	if (length % 2 != 0) fwrite(" ", 1, 1, zz->fp);	// pad with spaces
}
void zzwPN(struct zzfile *zz, zzKey key, const char *string) { wstr(zz, key, string, "PN", 64); }
void zzwSH(struct zzfile *zz, zzKey key, const char *string) { wstr(zz, key, string, "SH", 16); }
void zzwAE(struct zzfile *zz, zzKey key, const char *string) { wstr(zz, key, string, "AE", 16); }
void zzwAS(struct zzfile *zz, zzKey key, const char *string) { wstr(zz, key, string, "AS", 4); }
void zzwCS(struct zzfile *zz, zzKey key, const char *string) { wstr(zz, key, string, "CS", 16); }
void zzwLO(struct zzfile *zz, zzKey key, const char *string) { wstr(zz, key, string, "LO", 64); }
void zzwLT(struct zzfile *zz, zzKey key, const char *string) { wstr(zz, key, string, "LT", 10240); }

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
	zzwOB(zz, DCM_FileMetaInformationVersion, version, 2);
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

void zzwEmpty(struct zzfile *zz, zzKey key, const char *vr)
{
	const uint16_t group = ZZ_GROUP(key);
	const uint16_t element = ZZ_ELEMENT(key);

	if (explicit(zz)) explicit1(zz->fp, group, element, vr, 0);
	else implicit(zz->fp, group, element, 0);
}

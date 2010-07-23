#include <string.h>
#include <errno.h>

#include "zz_priv.h"
#include "byteorder.h"
#include "zzwrite.h"

// Magic values
#define MAGIC1 0xfffee00d
#define MAGIC2 BSWAP_32(0xfffee00d)
#define MAGIC3 0xfffee0dd
#define MAGIC4 BSWAP_32(0xfffee0dd)
#define MAGIC5 ((0x0010 << 24) | (0x0001 << 16) | ('S' << 8) | ('S'))

static inline bool explicit(struct zzfile *zz) { return zz->ladder[zz->ladderidx].txsyn == ZZ_EXPLICIT; }

static void implicit(FILE *fp, uint16_t group, uint16_t element, uint32_t length)
{
	fwrite(&group, 2, 1, fp);
	fwrite(&element, 2, 1, fp);
	fwrite(&length, 4, 1, fp);
}

static void genericfile(struct zzfile *zz, const char *sopclass)
{
	zzwUI(zz, DCM_SOPClassUID, sopclass);
	zzwUI(zz, DCM_SOPInstanceUID, "1.2.3.4.0");
	zzwEmpty(zz, DCM_StudyDate, "DA");
	zzwEmpty(zz, DCM_StudyTime, "TM");
	zzwSH(zz, DCM_AccessionNumber, "1234567890123456");
	zzwEmpty(zz, DCM_ReferringPhysiciansName, "PN");
	zzwEmpty(zz, DCM_PatientsName, "PN");
	zzwEmpty(zz, DCM_PatientID, "LO");
	zzwEmpty(zz, DCM_PatientsBirthDate, "DA");
	zzwEmpty(zz, DCM_PatientsSex, "CS");
	zzwUI(zz, DCM_StudyInstanceUID, "1.2.3.4.1");
	zzwUI(zz, DCM_SeriesInstanceUID, "1.2.3.4.2");
	zzwEmpty(zz, DCM_StudyID, "SH");
	zzwEmpty(zz, DCM_SeriesNumber, "IS");
	zzwEmpty(zz, DCM_InstanceNumber, "IS");
	zzwEmpty(zz, DCM_Laterality, "CS");
}

static void garbfill(struct zzfile *zz)
{
	switch (rand() % 5)
	{
	case 0: zzwUL(zz, DCM_DataPointRows, MAGIC1); zzwUL(zz, DCM_DataPointColumns, MAGIC2); break;
	case 1: zzwUL(zz, DCM_DataPointRows, MAGIC3); zzwUL(zz, DCM_DataPointColumns, MAGIC4); break;
	case 2: zzwUL(zz, DCM_DataPointRows, MAGIC2); zzwUL(zz, DCM_DataPointColumns, MAGIC5); break;
	case 3: zzwUL(zz, DCM_DataPointRows, MAGIC5); zzwUL(zz, DCM_DataPointColumns, MAGIC3); break;
	default:
	case 4: zzwUL(zz, DCM_DataPointRows, MAGIC4); zzwUL(zz, DCM_DataPointColumns, MAGIC1); break;
	}
}

static void zzwOBnoise(struct zzfile *zz, zzKey key)
{
	char *buf;
	size_t size;

	size = rand() % 9999;
	buf = malloc(size);
	memset(buf, rand(), size);
	if (!explicit(zz) && key == DCM_PixelData)
	{
		zzwOW(zz, key, buf, size / 2);
	}
	else
	{
		zzwOB(zz, key, buf, size);
	}
	free(buf);
}

int main(int argc, char **argv)
{
	const char *outputfile = "random.dcm";
	struct zzfile szz, *zz = &szz;

	(void)argc;
	(void)argv;

	zzutil(argc, argv, 1, "<random seed>", "Generate pseudo-random DICOM file for unit testing");
	if (argc > 1)
	{
		srand(atoi(argv[1]));
	}
	else
	{
		srand(1);
	}

	memset(zz, 0, sizeof(*zz));
	zz->fp = fopen(outputfile, "w");
	if (!zz->fp)
	{
		fprintf(stderr, "Failed to open output file \"%s\": %s\n", outputfile, strerror(errno));
	}

	switch (rand() % 15)
	{
	case 0: 
		zz->ladder[0].txsyn = ZZ_IMPLICIT;	// no header, implicit
		break;
	case 1:
		zz->ladder[0].txsyn = ZZ_IMPLICIT;	// implicit header, buggy like CTN
		zzwHeader(zz, UID_SecondaryCaptureImageStorage, "1.2.3.4.0", UID_LittleEndianImplicitTransferSyntax);
		break;
	case 2:
		zz->ladder[0].txsyn = ZZ_EXPLICIT;	// no header, explicit
		break;
	case 3:
		zz->ladder[0].txsyn = ZZ_IMPLICIT;	// implicit with explicit header
		zz->ladderidx = 1;
		zz->ladder[1].txsyn = ZZ_EXPLICIT;
		zzwHeader(zz, UID_SecondaryCaptureImageStorage, "1.2.3.4.0", UID_LittleEndianImplicitTransferSyntax);
		zz->ladderidx = 0;
		break;
	default:
		zz->ladder[0].txsyn = ZZ_EXPLICIT;	// explicit with explicit header
		zzwHeader(zz, UID_SecondaryCaptureImageStorage, "1.2.3.4.0", UID_LittleEndianExplicitTransferSyntax);
		break;
	}

	genericfile(zz, UID_SecondaryCaptureImageStorage);

	if (explicit(zz) && rand() % 10 > 2)	// add SQ block
	{
		zzwSQ(zz, ZZ_KEY(0x0020, 0x1115), UNLIMITED);
		implicit(zz->fp, 0xfffe, 0xe000, UNLIMITED);
		garbfill(zz);
		implicit(zz->fp, 0xfffe, 0xe00d, 0);
		implicit(zz->fp, 0xfffe, 0xe000, 24);
		garbfill(zz);
		if (rand() % 10 > 9) implicit(zz->fp, 0xfffe, 0xe00d, 0);	// this crashed dicom3tools; not really legal dicom
		implicit(zz->fp, 0xfffe, 0xe000, UNLIMITED);
		garbfill(zz);
		implicit(zz->fp, 0xfffe, 0xe00d, 0);
		implicit(zz->fp, 0xfffe, 0xe0dd, 0);
	}

	if (rand() % 10 > 5) garbfill(zz);

	if (explicit(zz) && rand() % 10 > 2)	// add UN block
	{
		zzwUN(zz, ZZ_KEY(0x0029, 0x0010), UNLIMITED);
		zz->ladderidx++;
		zz->ladder[zz->ladderidx].txsyn = ZZ_IMPLICIT;
		implicit(zz->fp, 0xfffe, 0xe000, UNLIMITED);
		garbfill(zz);
		implicit(zz->fp, 0xfffe, 0xe00d, 0);
		implicit(zz->fp, 0xfffe, 0xe000, UNLIMITED);
		garbfill(zz);
		implicit(zz->fp, 0xfffe, 0xe00d, 0);
		implicit(zz->fp, 0xfffe, 0xe0dd, 0);
		zz->ladderidx--;
	}

	if (rand() % 10 > 2) zzwOBnoise(zz, DCM_PixelData);
	if (rand() % 10 > 7) zzwOBnoise(zz, DCM_DataSetTrailingPadding);

	fclose(zz->fp);

	return 0;
}

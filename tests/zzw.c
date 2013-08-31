#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/time.h>

#include "byteorder.h"

#include "zzio.h"
#include "zz_priv.h"
#include "zzwrite.h"

// Magic values
#define MAGIC1 0xfffee00d
#define MAGIC2 BSWAP_32(0xfffee00d)
#define MAGIC3 0xfffee0dd
#define MAGIC4 BSWAP_32(0xfffee0dd)
#define MAGIC5 ((0x0010 << 24) | (0x0001 << 16) | ('S' << 8) | ('S'))

#define CHECK_GROUP 0x7001
#define CHECK_ELEM 0x1000
#define CHECK_VALUE 42

static bool checkContents(const char *testfile, bool reqperfection)
{
	struct zzfile szz, *zz;
	uint16_t group, element;
	long len;
	bool retval = false;

	// Now test reading it back in
	zz = zzopen(testfile, "r", &szz);
	zziterinit(zz);
	while (zziternext(zz, &group, &element, &len))
	{
		zzverify(zz);
		if (group == CHECK_GROUP && element == CHECK_ELEM && zzgetuint32(zz, 1) == CHECK_VALUE)
		{
			retval = true;
		}
		assert(!reqperfection || zz->current.valid);			// check that it is valid
		assert(zz->current.valid || zz->current.warning[0] != '\0');	// check that warning is set when invalid
	}
	assert(zz->currNesting == 0);
	assert(zz->ladderidx == 0);
	zz = zzclose(zz);
	return retval;
}

static inline bool explicit(struct zzfile *zz) { return zz->ladder[zz->ladderidx].txsyn == ZZ_EXPLICIT; }

static void implicit(struct zzio *zi, uint16_t group, uint16_t element, uint32_t length)
{
	ziwrite(zi, &group, 2);
	ziwrite(zi, &element, 2);
	ziwrite(zi, &length, 4);
}

static void genericfile(struct zzfile *zz)
{
	char str[MAX_LEN_UI];
	const double axial[] = { 1.0, 0.0, 0.0, 0.0, 1.0, 0.0 };

	memset(str, 0, sizeof(str));

	zzwUI(zz, DCM_SOPClassUID, UID_SecondaryCaptureImageStorage);
	zzwUI(zz, DCM_SOPInstanceUID, zzanonuid(str, sizeof(str) - 1));
	zzwEmpty(zz, DCM_StudyDate, DA);
	zzwEmpty(zz, DCM_StudyTime, TM);
	zzwSH(zz, DCM_AccessionNumber, "1234567890123456");
	zzwPN(zz, DCM_ReferringPhysiciansName, "Doctor^Johnson");
	zzwPN(zz, DCM_PatientsName, "Sick^Jack");
	zzwEmpty(zz, DCM_PatientID, LO);
	zzwEmpty(zz, DCM_PatientsBirthDate, DA);
	zzwEmpty(zz, DCM_PatientsSex, CS);
	zzwUI(zz, DCM_StudyInstanceUID, zzanonuid(str, sizeof(str) - 1));
	zzwUI(zz, DCM_SeriesInstanceUID, zzmakeuid(str, sizeof(str) - 1));
	zzwEmpty(zz, DCM_StudyID, SH);
	zzwEmpty(zz, DCM_SeriesNumber, IS);
	zzwEmpty(zz, DCM_InstanceNumber, IS);
	zzwDSdv(zz, DCM_ImageOrientationPatient, 6, axial);
	zzwEmpty(zz, DCM_Laterality, CS);
}

static void garbfill(struct zzfile *zz, int variant)
{
	switch (variant)
	{
	case 0: zzwUL(zz, DCM_DataPointRows, MAGIC1); zzwUL(zz, DCM_DataPointColumns, MAGIC2); break;
	case 1: zzwUL(zz, DCM_DataPointRows, MAGIC3); zzwUL(zz, DCM_DataPointColumns, MAGIC4); break;
	case 2: zzwUL(zz, DCM_DataPointRows, MAGIC2); zzwUL(zz, DCM_DataPointColumns, MAGIC5); break;
	case 3: zzwUL(zz, DCM_DataPointRows, MAGIC5); zzwUL(zz, DCM_DataPointColumns, MAGIC3); break;
	default:
	case 4: zzwUL(zz, DCM_DataPointRows, MAGIC4); zzwUL(zz, DCM_DataPointColumns, MAGIC1); break;
	}
}

static void zzwOBnoise(struct zzfile *zz, zzKey key, size_t size)
{
	char *buf;

	buf = malloc(size);
	memset(buf, 0, size);
	if (!explicit(zz) && key == DCM_PixelData)
	{
		zzwOW(zz, key, size / 2, (uint16_t *)buf);
	}
	else
	{
		zzwOB(zz, key, size, buf);
	}
	free(buf);
}

static void addCheck(struct zzfile *zz)
{
	const uint32_t valList[3] = { 0, CHECK_VALUE, 0 };
	zzwULv(zz, ZZ_KEY(CHECK_GROUP, CHECK_ELEM), 3, valList);
}

int main(int argc, char **argv)
{
	struct zzfile szz, *zz = &szz;
	bool result;
	long val;
	char str[MAX_LEN_UI];

	(void)argc;
	(void)argv;

	////
	// Set # 1 -- Deliberately confuse DICOM readers with this tiny, valid file with "DCM" in the wrong place

	memset(zz, 0, sizeof(*zz));
	memset(str, 0, sizeof(str));
	mkdir("samples", 0754);
	zz->zi = ziopenfile("samples/confuse.dcm", "w");
	if (!zz->zi)
	{
		fprintf(stderr, "Could not open output file: %s\n", strerror(errno));
	}
	zz->ladder[0].txsyn = ZZ_IMPLICIT;	// no header, implicit
	zzwUI(zz, DCM_SOPInstanceUID, zzmakeuid(str, sizeof(str) - 1));
	zzwEmpty(zz, DCM_StudyDate, DA);
	zzwEmpty(zz, DCM_StudyTime, TM);
	zzwSH(zz, DCM_AccessionNumber, "1234567890123456");
	zzwEmpty(zz, DCM_ReferringPhysiciansName, PN);
	zzwEmpty(zz, DCM_PatientsName, PN);
	zzwEmpty(zz, DCM_PatientID, LO);
	zzwEmpty(zz, DCM_PatientsBirthDate, DA);
	zzwEmpty(zz, DCM_PatientsSex, CS);
	zzwUI(zz, DCM_StudyInstanceUID, "1.2.3.4.1");
	zzwSH(zz, DCM_StudyID, "ANONDICMSTUDY");
	zzwEmpty(zz, DCM_Laterality, CS);
	addCheck(zz);
	ziclose(zz->zi);

	result = checkContents("samples/confuse.dcm", true);
	assert(result);

	////
	// Set # 2 -- Basic reading of part10 implicit

	zz = zzcreate("samples/tw1.dcm", &szz, UID_SecondaryCaptureImageStorage, "1.2.3.4.0", UID_LittleEndianImplicitTransferSyntax);
	genericfile(zz);
	zzwUN_begin(zz, ZZ_KEY(0x0029, 0x1010), NULL);
		implicit(zz->zi, 0xfffe, 0xe000, UNLIMITED);
		garbfill(zz, 0);
		implicit(zz->zi, 0xfffe, 0xe00d, 0);
		implicit(zz->zi, 0xfffe, 0xe000, UNLIMITED);
		garbfill(zz, 1);
		implicit(zz->zi, 0xfffe, 0xe00d, 0);
	zzwUN_end(zz, NULL);
	addCheck(zz);
	zz = zzclose(zz);

	result = checkContents("samples/tw1.dcm", true);
	assert(result);

	////
	// Set # 3 -- Basic reading of part10 explicit
	zz = zzcreate("samples/tw2.dcm", &szz, UID_SecondaryCaptureImageStorage, "1.2.3.4.0", UID_LittleEndianExplicitTransferSyntax);
	genericfile(zz);
	zzwSQ_begin(zz, ZZ_KEY(0x0020, 0x1115), NULL);
		implicit(zz->zi, 0xfffe, 0xe000, UNLIMITED);
		garbfill(zz, 0);
		implicit(zz->zi, 0xfffe, 0xe00d, 0);
		implicit(zz->zi, 0xfffe, 0xe000, 24);
		garbfill(zz, 1);
		implicit(zz->zi, 0xfffe, 0xe000, UNLIMITED);
		garbfill(zz, 2);
		implicit(zz->zi, 0xfffe, 0xe00d, 0);
		implicit(zz->zi, 0xfffe, 0xe000, UNLIMITED);
		zzwSS(zz, ZZ_KEY(0x0021, 0x1010), 11);
		zzwSL(zz, ZZ_KEY(0x0021, 0x1011), 12);
		zzwUS(zz, ZZ_KEY(0x0021, 0x1012), 13);
		zzwUL(zz, ZZ_KEY(0x0021, 0x1013), 14);
		zzwFL(zz, ZZ_KEY(0x0021, 0x1014), 15.0f);
		zzwFD(zz, ZZ_KEY(0x0021, 0x1015), 16.0);
		implicit(zz->zi, 0xfffe, 0xe00d, 0);
	zzwSQ_end(zz, NULL);
	zzwLO(zz, ZZ_KEY(0x0029, 0x0010), "ZZDICOM TEST");
	zzwUN_begin(zz, ZZ_KEY(0x0029, 0x1010), NULL);
		implicit(zz->zi, 0xfffe, 0xe000, UNLIMITED);
		garbfill(zz, 0);
		implicit(zz->zi, 0xfffe, 0xe00d, 0);
		implicit(zz->zi, 0xfffe, 0xe000, UNLIMITED);
		garbfill(zz, 1);
		implicit(zz->zi, 0xfffe, 0xe00d, 0);
	zzwUN_end(zz, NULL);
	zzwUN_begin(zz, ZZ_KEY(0x0029, 0x1010), &val);	// UN with specific size
		zzwItem_begin(zz, NULL);
		zzwItem_end(zz, NULL);
	zzwUN_end(zz, &val);
	addCheck(zz);
	zzwOBnoise(zz, DCM_PixelData, 1024);
	zzwOBnoise(zz, DCM_DataSetTrailingPadding, 256);
	zz = zzclose(zz);

	result = checkContents("samples/tw2.dcm", true);
	assert(result);

	////
	// Set # 4 -- Reading of broken part10 explicit

	zz = zzcreate("samples/brokensq.dcm", &szz, UID_SecondaryCaptureImageStorage, "1.2.3.4.0", UID_LittleEndianExplicitTransferSyntax);
	genericfile(zz);
	zzwSQ_begin(zz, ZZ_KEY(0x0020, 0x1115), NULL);
		implicit(zz->zi, 0xfffe, 0xe000, 200);	// bad size item...
		zzwSS(zz, ZZ_KEY(0x0021, 0x1010), 11);
		zzwSL(zz, ZZ_KEY(0x0021, 0x1011), 12);
		zzwUS(zz, ZZ_KEY(0x0021, 0x1012), 13);
		zzwUL(zz, ZZ_KEY(0x0021, 0x1013), 14);
		zzwFL(zz, ZZ_KEY(0x0021, 0x1014), 15.0f);
		zzwFD(zz, ZZ_KEY(0x0021, 0x1015), 16.0);
		implicit(zz->zi, 0xfffe, 0xe00d, 0);	// ...really ends here
		implicit(zz->zi, 0xfffe, 0xe000, 24);
		garbfill(zz, 1);
		implicit(zz->zi, 0xfffe, 0xe00d, 0);	// extra item end; this crashed dicom3tools, confuses dcmtk; not legal dicom; fun to do anyway
		implicit(zz->zi, 0xfffe, 0xe000, UNLIMITED);
		garbfill(zz, 2);
		implicit(zz->zi, 0xfffe, 0xe00d, 0);
	zzwSQ_end(zz, NULL);
	addCheck(zz);
	zzwOBnoise(zz, DCM_PixelData, 255);
	zzwOBnoise(zz, DCM_DataSetTrailingPadding, 0);
	zzwOBnoise(zz, DCM_DataSetTrailingPadding, 0); // duplicate tag
	zz = zzclose(zz);

	result = checkContents("samples/brokensq.dcm", false);
	assert(result);

	////
	// Set # 5 -- Part10 explicit with some really exotic stuff

	zz = zzcreate("samples/exotic.dcm", &szz, UID_SecondaryCaptureImageStorage, "1.2.3.4.0", UID_LittleEndianExplicitTransferSyntax);
	genericfile(zz);
	// Create bad size sequence
	{
		const uint16_t group = 0x0020;
		const uint16_t element = 0x1115;
		const char vr[] = "SQ";
		const uint16_t zero = 0;
		const uint32_t length = 2000;

		ziwrite(zz->zi, &group, 2);
		ziwrite(zz->zi, &element, 2);
		ziwrite(zz->zi, vr, 2);
		ziwrite(zz->zi, &zero, 2);
		ziwrite(zz->zi, &length, 4);
	}
	implicit(zz->zi, 0xfffe, 0xe0dd, 0);
	// Create group with wrong size
	zzwUL(zz, ZZ_KEY(0x0029, 0x0000), 4000);
	// Invent a new VR to check that the toolkit reads it correctly
	{
		const uint16_t group = 0x0029;
		const uint16_t element = 0x1010;
		const char vr[] = "QQ";
		const uint16_t zero = 0;
		const uint32_t length = 0;

		ziwrite(zz->zi, &group, 2);
		ziwrite(zz->zi, &element, 2);
		ziwrite(zz->zi, vr, 2);
		ziwrite(zz->zi, &zero, 2);
		ziwrite(zz->zi, &length, 4);
	}
	zzwUL(zz, ZZ_KEY(0x0031, 0x0012), 1); // check that warning for bad private creator is triggered
	zzwDA(zz, ZZ_KEY(0x0033, 0x1012), time(NULL));
	struct timeval tv;
	struct timezone tz;
	gettimeofday(&tv, &tz);
	zzwDT(zz, ZZ_KEY(0x0033, 0x1013), tv);
	zzwTM(zz, ZZ_KEY(0x0033, 0x1014), tv);
	addCheck(zz);
	zz = zzclose(zz);

	result = checkContents("samples/exotic.dcm", false);
	assert(result);

	return 0;
}

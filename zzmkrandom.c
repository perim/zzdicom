#include <string.h>
#include <errno.h>
#include <unistd.h>

#include "zz_priv.h"
#include "byteorder.h"
#include "zzwrite.h"

// Magic values
#define MAGIC1 0xfffee00d
#define MAGIC2 BSWAP_32(0xfffee00d)
#define MAGIC3 0xfffee0dd
#define MAGIC4 BSWAP_32(0xfffee0dd)
#define MAGIC5 ((0x0010 << 24) | (0x0001 << 16) | ('S' << 8) | ('S'))

static long zseed = 0; // stored in instance number
	
static inline bool explicit(struct zzfile *zz) { return zz->ladder[zz->ladderidx].txsyn == ZZ_EXPLICIT; }

static void implicit(struct zzio *zi, uint16_t group, uint16_t element, uint32_t length)
{
	ziwrite(zi, &group, 2);
	ziwrite(zi, &element, 2);
	ziwrite(zi, &length, 4);
}

static void genericfile(struct zzfile *zz, const char *sopclass)
{
	char uid[MAX_LEN_UI];
	zzwUI(zz, DCM_SOPClassUID, sopclass);
	zzwUI(zz, DCM_SOPInstanceUID, zzanonuid(uid, sizeof(uid)));
	if (rand() % 2 == 0) zzwEmpty(zz, DCM_StudyDate, DA);
	if (rand() % 2 == 0) zzwEmpty(zz, DCM_StudyTime, TM);
	if (rand() % 2 == 0) zzwSH(zz, DCM_AccessionNumber, "1234567890123456");
	if (rand() % 2 == 0) zzwEmpty(zz, DCM_ReferringPhysiciansName, PN);
	if (rand() % 2 == 0)
	{
		zzwEmpty(zz, DCM_PatientsName, PN);
	}
	else
	{
		zzwPN(zz, DCM_PatientsName, "random^zzmk");
	}
	zzwLO(zz, DCM_PatientID, "zzmkrandom"); // marker used for testing
	zzwEmpty(zz, DCM_PatientsBirthDate, DA);
	zzwEmpty(zz, DCM_PatientsSex, CS);
	if (rand() % 3 == 0)
		zzwUI(zz, DCM_StudyInstanceUID, "1.2.3.4.5.6.7.8.9.10.11.12.13.14.15.16.17.18.19.20.21.22.23.24.25.26");
	else
		zzwUI(zz, DCM_StudyInstanceUID, zzanonuid(uid, sizeof(uid)));
	zzwUI(zz, DCM_SeriesInstanceUID, "1.2.3.4.2");
	zzwEmpty(zz, DCM_StudyID, SH);
	if (rand() % 2 == 0)
		zzwEmpty(zz, DCM_SeriesNumber, IS);
	else
		zzwIS(zz, DCM_SeriesNumber, rand());
	zzwIS(zz, DCM_InstanceNumber, zseed);
	if (rand() % 2 == 0) zzwEmpty(zz, DCM_Laterality, CS);
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

static void zzwOBnoise(struct zzfile *zz, zzKey key, size_t size)
{
	char *buf;

	buf = malloc(size);
	memset(buf, rand(), size);
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

void addSQ(struct zzfile *zz)
{
	long i, loops, val, *pos = (rand() % 2) == 0 ? NULL : &val;
	zzwSQ_begin(zz, ZZ_KEY(0x0020, 0x1115), pos);
	loops = rand() % 8;
	for (i = 0; i < loops; i++)
	{
		long val2, *pos2 = (rand() % 2) == 0 ? NULL : &val2;
		zzwItem_begin(zz, pos2);
			if (rand() % 5 > 2) garbfill(zz);
		zzwItem_end(zz, pos2);
		if (pos2 != NULL && rand() % 10 > 8)
		{
			implicit(zz->zi, 0xfffe, 0xe00d, 0); // this crashed dicom3tools; not really legal dicom
		}
	}
	if (rand() % 3 == 0)
	{
		long val2, *pos2 = (rand() % 2) == 0 ? NULL : &val2;
		zzwItem_begin(zz, pos2);
		addSQ(zz);
		zzwItem_end(zz, pos2);
	}
	zzwSQ_end(zz, pos);
}

int main(int argc, char **argv)
{
	const char *outputfile = "random.dcm";
	struct zzfile szz, *zz = &szz;

	(void)argc;
	(void)argv;

	zzutil(argc, argv, 1, "<random seed>", "Generate pseudo-random DICOM file for unit testing", NULL);
	if (argc > 1)
	{
		zseed = atoi(argv[1]);
		if (argc > 2)
		{
			outputfile = argv[2];
		}
	}
	else
	{
		zseed = time(NULL) + getpid();
	}
	srand(zseed);

	memset(zz, 0, sizeof(*zz));
	zz->zi = ziopenfile(outputfile, "w");
	if (!zz->zi)
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
		addSQ(zz);
	}

	if (rand() % 10 > 5) garbfill(zz);

	// TODO, fix private tags namespace

	// try to confuse parsers that rely on arbitrary characters in the data stream - pretend to be an explicit VR
	if (!explicit(zz) && rand() % 10 > 8) zzwOBnoise(zz, ZZ_KEY(0x0029, 0x0009), 'S' + ('S' << 8));

	if (rand() % 10 > 2)	// add UN block
	{
		long val, *pos = (rand() % 2) == 0 ? NULL : &val;
		zzwUN_begin(zz, ZZ_KEY(0x0029, 0x1010), pos);
		if (rand() % 10 > 2)
		{
			long val2, *pos2 = (rand() % 2) == 0 ? NULL : &val2;
			zzwItem_begin(zz, pos2);
				if (rand() % 5 > 2) garbfill(zz);
			zzwItem_end(zz, pos2);
			if (rand() % 10 > 2)
			{
				long val3, *pos3 = (rand() % 2) == 0 ? NULL : &val3;
				zzwItem_begin(zz, pos3);
					if (rand() % 5 > 2) garbfill(zz);
				zzwItem_end(zz, pos3);
			}
		}
		zzwUN_end(zz, NULL);
	}

	// Invent a new VR to check that the toolkit reads it correctly
	if (explicit(zz) && rand() % 5 == 0)
	{
		const uint16_t group = 0x0029;
		const uint16_t element = 0x1090;
		const uint16_t zero = 0;
		const uint32_t length = 0;

		ziwrite(zz->zi, &group, 2);
		ziwrite(zz->zi, &element, 2);
		ziwrite(zz->zi, "QQ", 2);
		ziwrite(zz->zi, &zero, 2);
		ziwrite(zz->zi, &length, 4);
	}

	if (rand() % 10 > 3) zzwOBnoise(zz, DCM_PixelData, rand() % 9999);
	if (rand() % 10 > 7) zzwOBnoise(zz, DCM_DataSetTrailingPadding, rand() % 9999);

	ziclose(zz->zi);

	return 0;
}

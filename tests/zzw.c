#include <assert.h>
#include <string.h>
#include <errno.h>

#include "zz_priv.h"
#include "zzwrite.h"

static bool checkContents(const char *testfile)
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
		if (ZZ_KEY(group, element) == DCM_StudyID)
		{
			retval = true;
		}
	}
	assert(zz->currNesting == 0);
	zz = zzclose(zz);
	return retval;
}

int main(int argc, char **argv)
{
	struct zzfile szz, *zz = &szz;
	bool result;

	(void)argc;
	(void)argv;

	////
	// Set # 1 -- Deliberately confuse DICOM readers with this tiny, valid file with "DCM" in the wrong place

	memset(zz, 0, sizeof(*zz));
	zz->fp = fopen("confuse.dcm", "w");
	zz->ladder[0].txsyn = ZZ_IMPLICIT;	// no header, implicit
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
	zzwSH(zz, DCM_StudyID, "ANONDICMSTUDY");
	zzwEmpty(zz, DCM_Laterality, "CS");
	fclose(zz->fp);

	result = checkContents("confuse.dcm");
	assert(result);

	////
	// Set # 2 -- Basic reading

	zz = zzcreate("tw1.dcm", &szz, UID_SecondaryCaptureImageStorage, "1.2.3.4.0", UID_LittleEndianImplicitTransferSyntax);
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
	zzwSH(zz, DCM_StudyID, "TEST STUDY");
	zzwEmpty(zz, DCM_Laterality, "CS");
	zz = zzclose(zz);

	result = checkContents("tw1.dcm");
	assert(result);

	return 0;
}

#include <string.h>
#include <errno.h>

#include "zz_priv.h"
#include "zzwrite.h"

int main(int argc, char **argv)
{
	const char *outputfile = "tw1.dcm";
	struct zzfile szz, *zz = &szz;

	(void)argc;
	(void)argv;

	memset(zz, 0, sizeof(*zz));
	zz->fp = fopen(outputfile, "w");
	if (!zz->fp)
	{
		fprintf(stderr, "Failed to open output file \"%s\": %s\n", outputfile, strerror(errno));
	}
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

	return 0;
}

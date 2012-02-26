#include <assert.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "zz_priv.h"

static void checkinvalid(const char *path, uint16_t cgroup, uint16_t celement)
{
	struct zzfile szz, *zz;
	uint16_t group, element;
	long len;
	bool retval = false;
	char buf[128];

	memset(buf, 0, sizeof(buf));
	zz = zzopen(path, "r", &szz);
	zziterinit(zz);
	while (zziternext(zz, &group, &element, &len))
	{
		zzverify(zz);
		if (group == cgroup && element == celement)
		{
			assert(!zz->current.valid);
		}
	}
	assert(zz->currNesting == 0);
	assert(zz->ladderidx == 0);
	zz = zzclose(zz);
}

static void checkfile(const char *path, uint16_t cgroup, uint16_t celement, const char *content, bool reqvalid)
{
	struct zzfile szz, *zz;
	uint16_t group, element;
	long len;
	bool retval = false;
	char buf[128];

	memset(buf, 0, sizeof(buf));
	zz = zzopen(path, "r", &szz);
	zziterinit(zz);
	while (zziternext(zz, &group, &element, &len))
	{
		zzverify(zz);
		assert(!reqvalid || zz->current.valid);
		if (group == cgroup && element == celement)
		{
			zzgetstring(zz, buf, sizeof(buf) - 1);
			assert(strcmp(buf, content) == 0);
		}
	}
	assert(zz->currNesting == 0);
	assert(zz->ladderidx == 0);
	zz = zzclose(zz);
}

int main(void)
{
	checkfile("gdcmData/CT-SIEMENS-Icone-With-PaletteColor.dcm", 0x0029, 0x1134, "DB TO DICOM", true);
	checkfile("gdcmData/SIEMENS_CSA2.dcm", 0x0095, 0x0010, "SIENET", true);
	checkfile("gdcmData/D_CLUNIE_CT1_JLSN.dcm", 0x0043, 0x104e, "10.600610", true);
	checkfile("gdcmData/AMIInvalidPrivateDefinedLengthSQasUN.dcm", 0x7fd1, 0x0010, "GEIIS", true);
	checkfile("gdcmData/BugGDCM2_UndefItemWrongVL.dcm", 0x0028, 0x1051, "1037", true);

	checkinvalid("gdcmData/THERALYS-12-MONO2-Uncompressed-Even_Length_Tag.dcm", 0x0008, 0x0070);

	return 0;
}

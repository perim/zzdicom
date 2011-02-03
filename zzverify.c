#include "zz_priv.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>

bool zzverify(struct zzfile *zz)
{
	zzKey key = ZZ_KEY(zz->current.group, zz->current.element);
	double tmpd[4];

	if (zz->current.length > 0 && zz->current.length != UNLIMITED && zz->current.pos + zz->current.length > zz->fileSize)
	{
		sprintf(zz->current.warning, "Data size %ld exceeds file end\n", zz->current.length);
		zz->current.valid = false;
		return false;
	}
	if (zz->current.group != 0xfffe && zz->previous.group != 0xfffe && zz->previous.ladderidx == zz->ladderidx)
	{
		if (zz->current.group == zz->previous.group && zz->current.element <= zz->previous.element)
		{
			zz->current.valid = false;
			strcpy(zz->current.warning, "Out of order tag");
		}
		if (zz->current.group < zz->previous.group)
		{
			zz->current.valid = false;
			strcpy(zz->current.warning, "Out of order group");
		}
	}
	switch (key)
	{
	case DCM_SliceThickness:
		zzrDS(zz, 1, tmpd);
		if (tmpd[0] < 0.0)
		{
			sprintf(zz->current.warning, "Negative slice thickness not allowed");
			zz->current.valid = false;
		}
		break;
	case DCM_PixelSpacing:
		zzrDS(zz, 2, tmpd);
		if (tmpd[0] < 0.0 || tmpd[1] < 0.0)
		{
			sprintf(zz->current.warning, "Negative pixel spacing not allowed");
			zz->current.valid = false;
		}
		break;
	default:
		break;
	}
	if (zz->current.length != UNLIMITED && zz->current.length % 2 != 0)	// really dumb DICOM requirement, but hey, it's there...
	{
		sprintf(zz->current.warning, "Data size is not even");
		zz->current.valid = false;
	}
	return zz->current.valid;
}

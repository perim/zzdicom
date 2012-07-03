#include "zz_priv.h"

#include <unistd.h>
#include <string.h>
#include <ctype.h>

bool zzverify(struct zzfile *zz)
{
	zzKey key = ZZ_KEY(zz->current.group, zz->current.element);
	double tmpd[4];

	if (zz->current.length > 0 && zz->current.length != UNLIMITED && zz->current.pos + zz->current.length > zz->fileSize)
	{
		sprintf(zz->current.warning, "Data size %ld exceeds file end, %ld bytes left in file", 
		        zz->current.length, zz->fileSize - zz->current.pos);
		zz->current.valid = false;
		return false;
	}
	if (zz->current.group != 0xfffe && zz->previous.group != 0xffff && zz->previous.group != 0xfffe && zz->previous.ladderidx == zz->ladderidx)
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
	if (zz->current.group % 2 > 0 && zz->current.element < 0x1000 && zz->current.element > 0x0010
	    && zz->current.vr != LO && zz->current.vr != NO)
	{
		sprintf(zz->current.warning, "Private creator tag must be value representation LO");
		zz->current.valid = false;
	}
	switch (key)
	{
	case DCM_Item:
		if (zz->current.pxstate == ZZ_PIXELITEM && zz->pxOffsetTable > 0)
		{
			uint32_t offstored;
			long offactual;
			long curr = zz->current.pos;

			zisetreadpos(zz->zi, zz->pxOffsetTable + zz->current.frame * sizeof(offstored));
			ziread(zz->zi, &offstored, sizeof(offstored));
			zisetreadpos(zz->zi, curr);
			offactual = curr - (zz->pxOffsetTable + sizeof(offstored) * zz->frames + 8);
			if ((long)offstored != offactual)
			{
				sprintf(zz->current.warning, "Frame offset %ld different than actual frame position %ld", (long)offstored, offactual);
				zz->current.valid = false;
			}
		}
		else if (zz->current.frame >= 0 && zz->current.frame >= zz->frames && zz->frames >= 0)	// inside a per-frame sequence
		{
			sprintf(zz->current.warning, "More per-frame sequence items than frames (item %ld, %ld frames)", zz->current.frame, zz->frames);
			zz->current.valid = false;
		}
		break;
	case DCM_WindowWidth:
		zzrDS(zz, 1, tmpd);
		if (tmpd[0] < 0.0)
		{
			sprintf(zz->current.warning, "Value must be 1 or higher");
			zz->current.valid = false;
		}
		break;
	case DCM_SliceThickness:
		zzrDS(zz, 1, tmpd);
		if (tmpd[0] < 0.0)
		{
			sprintf(zz->current.warning, "Negative value not allowed");
			zz->current.valid = false;
		}
		break;
	case DCM_PixelSpacing:
		zzrDS(zz, 2, tmpd);
		if (tmpd[0] < 0.0 || tmpd[1] < 0.0)
		{
			sprintf(zz->current.warning, "Negative value not allowed");
			zz->current.valid = false;
		}
		break;
	default:
		break;
	}
	if (zz->current.valid && zz->current.length != UNLIMITED && zz->current.length % 2 != 0)
	{
		// really dumb DICOM requirement, but it's there and some toolkits treat it as a terminal error...
		sprintf(zz->current.warning, "Data size is not even");
		zz->current.valid = false;
	}
	return zz->current.valid;
}

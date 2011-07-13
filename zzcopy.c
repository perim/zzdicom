#include "zz_priv.h"
#include "zzwrite.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "part6.h"

void copy(const char *destination, const char *source)
{
	struct zzfile szzdst, szzsrc, *dst = NULL, *src;
	uint16_t group, element;
	long len;
	const char *vm, *description;
	const struct part6 *tag;
	const struct privatedic *privtag;
	char vrstr[3], longstr[80];

	src = zzopen(source, "r", &szzsrc);
	if (!src)
	{
		exit(1);
	}
	zziterinit(src);
	while (zziternext(src, &group, &element, &len))
	{
		if (group > 0x002)
		{
			if (!dst)
			{
				dst = zzcreate(destination, &szzdst, src->sopClassUid, src->sopInstanceUid, UID_LittleEndianExplicitTransferSyntax);
			}
			tag = zztag(group, element);
			if ((src->current.vr == NO || src->current.vr == UN) && tag && src->ladder[src->ladderidx].txsyn == ZZ_IMPLICIT && group != 0xfffe)
			{
				src->current.vr = ZZ_VR(tag->VR[0], tag->VR[1]);
			}
			else if ((src->current.vr == NO && group != 0xfffe) || src->current.vr == UN)
			{
				if (src->current.length == UNLIMITED)
				{
					src->current.vr = SQ;
				}
				else
				{
					src->current.vr = UN;
					dst->ladder[dst->ladderidx].txsyn = ZZ_IMPLICIT; // This will not work yet... FIXME
				}
			}
			zzwCopy(dst, src);
		}
	}
	src = zzclose(src);
	dst = zzclose(dst);
}

int main(int argc, char **argv)
{
	zzutil(argc, argv, 3, "<source> <destination>", "DICOM file copy and convert");
	copy(argv[2], argv[1]);
	return 0;
}

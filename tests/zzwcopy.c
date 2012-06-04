#include <assert.h>
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
	const struct part6 *tag;

	src = zzopen(source, "r", &szzsrc);
	assert(src);
	zziterinit(src);
	while (zziternext(src, &group, &element, &len))
	{
		if (group > 0x002) // Skip header, as we want to recreate it
		{
			if (!dst)
			{
				dst = zzcreate(destination, &szzdst, src->sopClassUid, src->sopInstanceUid, UID_LittleEndianExplicitTransferSyntax);
				assert(dst);
			}
			tag = zztag(group, element);
			if ((src->current.vr == NO || src->current.vr == UN) && tag && src->ladder[src->ladderidx].txsyn == ZZ_IMPLICIT && group != 0xfffe)
			{
				src->current.vr = ZZ_VR(tag->VR[0], tag->VR[1]);
			}
			else if ((src->current.vr == NO && group != 0xfffe) || src->current.vr == UN)
			{
				assert(src->current.length == UNLIMITED); // otherwise test won't work
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

	// -- Verify step

	src = zzopen(destination, "r", &szzsrc);
	zziterinit(src);
	while (zziternext(src, &group, &element, &len))
	{
		zzverify(src);
		if (!src->current.valid)
		{
			printf("Error in (%04x, %04x) length %ld: %s\n", group, element, len, src->current.warning);
		}
		assert(src->current.valid);
		if (group == 0x0010 && element == 0x0020)
		{
			char buf[80];
			assert(strcmp(zzgetstring(src, buf, sizeof(buf) - 1), "zzmkrandom") == 0);
		}
	}
	src = zzclose(src);
}

int main(void)
{
	copy("samples/zzwcopy.dcm", "samples/random.dcm");
	return 0;
}

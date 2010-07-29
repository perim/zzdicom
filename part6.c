#include "zz.h"
#include "zz_priv.h"

static const struct part6 part6_table[] = 
{
#include "part6.h"
};

const struct part6 *zztag(uint16_t group, uint16_t element)
{
	// Since we read DICOM files sequentially by ascending group/element tags, assuming the same for 
	// tag lookups much improves lookup speed. Hence the lastidx hack.
	static int lastidx = 0;
	const int max = ARRAY_SIZE(part6_table);
	int i;

	for (i = lastidx; i < max; i++)
	{
		if (group == part6_table[i].group && element == part6_table[i].element)
		{
			lastidx = i;
			return &part6_table[i];
		}
	}
	for (i = 0; i < lastidx; i++)
	{
		if (group == part6_table[i].group && element == part6_table[i].element)
		{
			lastidx = i;
			return &part6_table[i];
		}
	}
	return NULL;
}


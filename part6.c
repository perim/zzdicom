#include "zz.h"
#include "zz_priv.h"
#include "part6.h"

static const struct privatedic private_table[] =
{
#include "private_c.h"
};

static const struct part6 part6_table[] = 
{
#include "part6_c.h"
};

const struct privatedic *zzprivtag(uint16_t group, uint16_t element, const char *label)
{
	unsigned int i;

	(void)label;
	// Ok, now the difficult and slow case -- we did not find it in our normal dictionary. Try our
	// privat dictionary instead?
	// TODO handle private group offsets
	for (i = 0; i < ARRAY_SIZE(private_table); i++)
	{
		if (group == private_table[i].group && element == 0x1000 + private_table[i].element)
		{
			return &private_table[i];
		}
	}
	return NULL;
}

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


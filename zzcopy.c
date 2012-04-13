#include "zz_priv.h"
#include "zzwrite.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#include "part6.h"

enum
{
	OPT_RGB,
	OPT_COUNT
};

static struct zzopts opts[] =
	{ { "--rgb", "Convert to RGB", false, false, 0, 0 },	// OPT_RGB
	  { NULL, NULL, false, false, 0, 0 } };		// OPT_COUNT

void copy(const char *destination, const char *source)
{
	struct zzfile szzdst, szzsrc, *dst = NULL, *src;
	uint16_t group, element;
	long len;
	const char *vm, *description;
	const struct part6 *tag;
	const struct privatedic *privtag;
	char vrstr[3], longstr[80];
	long samples_per_pixel = 1, x = 0, y = 0, z = 0;
	char value[MAX_LEN_IS];

	src = zzopen(source, "r", &szzsrc);
	if (!src)
	{
		exit(1);
	}
	zziterinit(src);
	while (zziternext(src, &group, &element, &len))
	{
		if (group > 0x002) // Skip header, as we want to recreate it
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

			switch (ZZ_KEY(group, element))
			{
			case DCM_SamplesPerPixel:
				samples_per_pixel = zzgetuint16(src, 0);
				if (opts[OPT_RGB].found)
				{
					zzwUS(dst, DCM_SamplesPerPixel, 3);
					break;
				}
				zzwCopy(dst, src);
				break;
			case DCM_BitsStored:
				if (opts[OPT_RGB].found)
				{
					zzwUS(dst, DCM_BitsStored, 8);
					break;
				}
				zzwCopy(dst, src);
				break;
			case DCM_BitsAllocated:
				if (opts[OPT_RGB].found)
				{
					zzwUS(dst, DCM_BitsAllocated, 8);
					break;
				}
				zzwCopy(dst, src);
				break;
			case DCM_HighBit:
				if (opts[OPT_RGB].found)
				{
					zzwUS(dst, DCM_BitsStored, 7);
					break;
				}
				zzwCopy(dst, src);
				break;
			case DCM_PhotometricInterpretation:
				if (opts[OPT_RGB].found)
				{
					zzwCS(dst, DCM_PhotometricInterpretation, "RGB");
					break;
				}
				zzwCopy(dst, src);
				break;
			case DCM_NumberOfFrames:
				zzgetstring(src, value, sizeof(value) - 1);
				z = atoi(value);
				zzwCopy(dst, src);
				break;
			case DCM_Rows:
				y = zzgetuint16(src, 0);
				zzwCopy(dst, src);
				break;
			case DCM_Columns:
				x = zzgetuint16(src, 0);
				zzwCopy(dst, src);
				break;
			case DCM_PixelData:
				if (opts[OPT_RGB].found && samples_per_pixel != 3)
				{
					char *rgb;
					uint16_t *mono;
					long i, srcsize, dstsize;

					if (samples_per_pixel != 1)
					{
						fprintf(stderr, "Samples per pixel %ld not supported!", samples_per_pixel);
						exit(1);
					}
					if (src->ladder[src->ladderidx].txsyn == ZZ_EXPLICIT_JPEGLS) // TODO check more
					{
						fprintf(stderr, "Compressed pixel data not supported!");
						exit(1);
					}
					srcsize = x * y * z;
					dstsize = srcsize * 3;
					rgb = malloc(dstsize);
					mono = zireadbuf(src->zi, srcsize * 2);
					// Copy pixels, make highest intensity red
					for (i = 0; i < srcsize; i++)
					{
						const int intensity = mono[i] >> 8;
						if (intensity < 250)
						{
							rgb[i * 3 + 0] = intensity;
							rgb[i * 3 + 0] = intensity;
							rgb[i * 3 + 0] = intensity;
						}
						else
						{
							rgb[i * 3 + 0] = 255;
							rgb[i * 3 + 0] = 0;
							rgb[i * 3 + 0] = 0;
						}
					}
					zifreebuf(src->zi, mono, srcsize * 2);
					zzwOB(dst, DCM_PixelData, dstsize, (const char *)rgb);
					free(rgb);
					break;
				}
				zzwCopy(dst, src);
				break;
			default:
				zzwCopy(dst, src);
				break;
			}
		}
	}
	src = zzclose(src);
	dst = zzclose(dst);
}

int main(int argc, char **argv)
{
	int firstparam = zzutil(argc, argv, 2, "<source> <destination>", "DICOM file copy and convert", opts);
	copy(argv[firstparam + 1], argv[firstparam]);
	return 0;
}

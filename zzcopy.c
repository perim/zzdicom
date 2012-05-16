#include "zz_priv.h"
#include "zzwrite.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <CharLS/interface.h>

#include "part6.h"

enum
{
	OPT_JPEGLS,
	OPT_RGB,
	OPT_COUNT
};

static struct zzopts opts[] =
	{ { "--jpegls", "Convert to JPEG-lS compression", false, false, 0, 0 },	// OPT_JPEGLS
	  { "--rgb", "Convert to RGB", false, false, 0, 0 },	// OPT_RGB
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
	long samples_per_pixel = 1, x = 0, y = 0, z = 0, bits_per_sample = 16;
	char value[MAX_LEN_IS];
	char uid[MAX_LEN_UI];

	zzmakeuid(uid, sizeof(uid));	// make new SOP instance UID
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
				const char *ts = opts[OPT_JPEGLS].found ?
					UID_JPEGLSLosslessTransferSyntax : UID_LittleEndianExplicitTransferSyntax;
				dst = zzcreate(destination, &szzdst, src->sopClassUid, uid, ts);
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
			case DCM_SOPInstanceUID: 
				zzwUI(dst, DCM_SOPInstanceUID, uid);		// add new SOP instance UID
				break;
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
				bits_per_sample = zzgetuint16(src, 0);
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
				// FIXME, handle frame table...  then transforms to apply to it
				if (opts[OPT_JPEGLS].found)
				{
					void *srcbuf = zireadbuf(src->zi, len);
					const int size = x * y * (bits_per_sample / 8) * samples_per_pixel;
					void *dstbuf = malloc(size);	// assume that we will not compress worse
					struct JlsParameters params;
					enum JLS_ERROR err;
					size_t result;
					int i;

					memset(&params, 0, sizeof(params));
					params.bitspersample = bits_per_sample;
					params.height = x;
					params.width = y;
					params.components = samples_per_pixel;
					params.ilv = (params.components == 3) ? ILV_SAMPLE : ILV_NONE;
					params.bytesperline = 0;
					dst->ladder[dst->ladderidx].txsyn = ZZ_EXPLICIT_JPEGLS;
					zzwPixelData_begin(dst, z, bits_per_sample, UNLIMITED);
					for (i = 0; i < z; i++)
					{
						result = 0;
						err = JpegLsEncode(dstbuf, size, &result, srcbuf + i * size, size, &params);
						switch (err)
						{
						case OK: break;
						case TooMuchCompressedData: fprintf(stderr, "%s - too much compressed data\n", source); break;
						case InvalidJlsParameters: fprintf(stderr, "%s - invalid encoding parameters\n", source); break;
						case ParameterValueNotSupported: fprintf(stderr, "%s - not supported encoding parameters\n", source); break;
						case UncompressedBufferTooSmall: fprintf(stderr, "%s - could not yield compression gain\n", source); break;
						case InvalidCompressedData:
						case ImageTypeNotSupported:
						case CompressedBufferTooSmall: fprintf(stderr, "%s - this should not happen\n", source); break;
						case UnsupportedBitDepthForTransform: fprintf(stderr, "%s - unsupported bit depth\n", source); break;
						case UnsupportedColorTransform: fprintf(stderr, "%s - unsupported color transform\n", source); break;
						};
						zzwPixelData_frame(dst, i, dstbuf, result);
					}
					zzwPixelData_end(dst);
					free(dstbuf);
					zifreebuf(src->zi, srcbuf, len);
				}
				else if (opts[OPT_RGB].found && samples_per_pixel != 3)
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
				}
				else
				{
					zzwCopy(dst, src);
				}
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

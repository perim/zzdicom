#include "zz_priv.h"

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <sys/mman.h>
#include <errno.h>

#include "part6.h"

enum
{
	OPT_ZERO,
	OPT_COUNT
};

static struct zzopts opts[] =
	{ { "--zero <x1> <y1> <x2> <y2>", "Fill region of image with zeroes", false, false, 4, 0 }, // OPT_ZERO
	  { NULL, NULL, false, false, 0, 0 } };              // OPT_COUNT

// zero out pixels in a 8bit black/white image
void zero8bbw(uint8_t *data, int width, int height, int depth, int bx1, int by1, int bx2, int by2)
{
	int x, y, z;
	for (x = 0; x < width; x++)
	{
		for (y = 0; y < height; y++)
		{
			for (z = 0; z < depth; z++)
			{
				if (x >= bx1 && x < bx2 && y >= by1 && y < by2)
				{
					data[z * width * height + y * width + x] = 0;
				}
			}
		}
	}
}

// zero out pixels in a 16bit black/white image
void zero16bbw(uint16_t *data, int width, int height, int depth, int bx1, int by1, int bx2, int by2)
{
	int x, y, z;
	for (x = 0; x < width; x++)
	{
		for (y = 0; y < height; y++)
		{
			for (z = 0; z < depth; z++)
			{
				if (x >= bx1 && x < bx2 && y >= by1 && y < by2)
				{
					data[z * width * height + y * width + x] = 0;
				}
			}
		}
	}
}

void manip(const char *source, int bx1, int by1, int bx2, int by2)
{
	struct zzfile szzsrc, *src;
	uint16_t group, element;
	long len;
	const struct part6 *tag;
	long samples_per_pixel = 1, width = 0, height = 0, depth = 1, bits_per_sample = 16;
	char value[MAX_LEN_IS];

	src = zzopen(source, "rw", &szzsrc);
	if (!src)
	{
		exit(EXIT_FAILURE);
	}
	zziterinit(src);
	while (zziternext(src, &group, &element, &len))
	{
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
			}
		}

		switch (ZZ_KEY(group, element))
		{
		case DCM_SamplesPerPixel:
			samples_per_pixel = zzgetuint16(src, 0);
			break;
		case DCM_BitsStored:
			break;
		case DCM_BitsAllocated:
			bits_per_sample = zzgetuint16(src, 0);
			break;
		case DCM_HighBit:
			break;
		case DCM_PhotometricInterpretation:
			break;
		case DCM_NumberOfFrames:
			zzgetstring(src, value, sizeof(value) - 1);
			depth = atoi(value);
			break;
		case DCM_Rows:
			height = zzgetuint16(src, 0);
			break;
		case DCM_Columns:
			width = zzgetuint16(src, 0);
			break;
		case DCM_PixelData:
		{
			const int bytes_per_sample = 2;
			const long size = width * height * depth * bytes_per_sample;
			long i;
			long pos = zireadpos(src->zi);
			long offset = pos & ~(sysconf(_SC_PAGE_SIZE) - 1);	// start at page aligned offset
			void *addr;

			if (samples_per_pixel != 1)
			{
				fprintf(stderr, "Samples per pixel %ld not supported!\n", samples_per_pixel);
				exit(EXIT_FAILURE);
			}
			if (bits_per_sample != 8 && bits_per_sample != 16)
			{
				fprintf(stderr, "Bad bits per sample: %ld\n", bits_per_sample);
				exit(EXIT_FAILURE);
			}
			if (src->ladder[src->ladderidx].txsyn == ZZ_EXPLICIT_JPEGLS) // TODO check more
			{
				fprintf(stderr, "Compressed pixel data not supported!\n");
				exit(EXIT_FAILURE);
			}

			// don't parse an icon sequence's image data
			for (i = 0; i < src->ladderidx; i++)
			{
				if (src->ladder[i].type == ZZ_SEQUENCE)
				{
					if (ZZ_KEY(src->ladder[i].group, src->ladder[i].element) == DCM_IconImageSequence)
					{
						i = -1;
						break;
					}
				}
			}
			if (i == -1)
			{
				break;
			}

			addr = mmap(NULL, size + pos - offset, PROT_READ | PROT_WRITE, MAP_SHARED, zifd(src->zi), offset);
			if (addr == MAP_FAILED)
			{
				fprintf(stderr, "Failed to map image data: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			if (madvise(addr, size + pos - offset, MADV_SEQUENTIAL) != 0)
			{
				fprintf(stderr, "Failed to advise kernel on image buffer: %s\n", strerror(errno));
			}

			if (opts[OPT_ZERO].found)
			{
				if (bits_per_sample == 16)
				{
					zero16bbw(addr + pos - offset, width, height, depth, bx1, by1, bx2, by2);
				}
				else if (bits_per_sample == 8)
				{
					zero8bbw(addr + pos - offset, width, height, depth, bx1, by1, bx2, by2);
				}
			}

			if (msync(addr, size + pos - offset, MS_ASYNC) != 0)
			{
				fprintf(stderr, "Failed to sync changes: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			if (munmap(addr, size + pos - offset) != 0)
			{
				fprintf(stderr, "Failed to unmap image data: %s\n", strerror(errno));
				exit(EXIT_FAILURE);
			}
			break;
		}
		default:
			break;
		}
	}
	src = zzclose(src);
}

int main(int argc, char **argv)
{
	int firstparam = zzutil(argc, argv, 1, "<source>", "DICOM file pixel manipulation", opts);
	if (opts[OPT_ZERO].found)
	{
		int param = opts[OPT_ZERO].argstart;
		int x1 = atoi(argv[param + 1]);
		int y1 = atoi(argv[param + 2]);
		int x2 = atoi(argv[param + 3]);
		int y2 = atoi(argv[param + 4]);
		manip(argv[firstparam], x1, y1, x2, y2);
	}
	else
	{
		fprintf(stdout, "Nothing to do\n");
	}
	return EXIT_SUCCESS;
}

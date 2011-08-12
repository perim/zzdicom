#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <sys/mman.h>
#include <CharLS/interface.h>

#include "zz_priv.h"
#include "zzsql.h"
#include "zzwrite.h"
#include "part6.h"

static void explicit1(FILE *fp, uint16_t group, uint16_t element, const char *vr, uint16_t length)
{
        fwrite(&group, 2, 1, fp);
        fwrite(&element, 2, 1, fp);
        fwrite(&vr[0], 1, 1, fp);
        fwrite(&vr[1], 1, 1, fp);
        fwrite(&length, 2, 1, fp);
}

static void explicit2(FILE *fp, uint16_t group, uint16_t element, const char *vr, uint32_t length)
{
        uint16_t zero = 0;

        fwrite(&group, 2, 1, fp);
        fwrite(&element, 2, 1, fp);
        fwrite(&vr[0], 1, 1, fp);
        fwrite(&vr[1], 1, 1, fp);
        fwrite(&zero, 2, 1, fp);
        fwrite(&length, 4, 1, fp);
}

static bool jpegtols(char *filename)
{
	struct JlsParameters params;
	enum JLS_ERROR err;
	struct zzfile szw, *zw;
	struct zzfile szz, *zz = zzopen(filename, "r", &szz);
	struct stat st;
	size_t result;
	uint16_t group, element;
	char *src, *dst;
	long len, size;
	char newname[PATH_MAX], *cptr, vrstr[MAX_LEN_VR];
	void *addr;
	const struct part6 *tag;

	if (!zz)
	{
		fprintf(stderr, "Failed to open file %s", filename);
		exit(-1);
	}
	fstat(fileno(zz->fp), &st);
	size = st.st_size;
	strcpy(newname, zz->fullPath);
	cptr = strrchr(newname, '.');
	if (cptr && cptr > strrchr(newname, '/')) *cptr = '\0';	// chop off extension, looks weird otherwise; make sure we don't clobber path
	if (strlen(newname) > PATH_MAX - 4)
	{
		fprintf(stderr, "%s - filename too long\n", newname);
		return false;
	}
	strcat(newname, "-ls");
	zw = zzcreate(newname, &szw, zz->sopClassUid, zz->sopInstanceUid, UID_JPEGLSLosslessTransferSyntax);
	if (!zw)
	{
		fprintf(stderr, "%s - could not create out file: %s\n", newname, strerror(errno));
		zz = zzclose(zz);
		return false;
	}
	// memory map the entire file to avoid the hassle of dynamic memory management
	addr = mmap(NULL, zz->fileSize, PROT_READ, MAP_SHARED, fileno(zz->fp), 0);
	if (addr == MAP_FAILED)
	{
		fprintf(stderr, "%s - could not mmap file: %s\n", filename, strerror(errno));
		zz = zzclose(zz);
		return false;
	}

	memset(&params, 0, sizeof(params));

	zziterinit(zz);
	while (zziternext(zz, &group, &element, &len))
	{
		zzKey key = ZZ_KEY(group, element);

		if (group > 0x0002 && key != DCM_PixelData && element != 0)
		{
			const char *vr = zzvr2str(zz->current.vr, vrstr);

			if (zz->current.vr == NO)
			{
				tag = zztag(group, element);
				if (tag)
				{
					vr = tag->VR;
				}
			}

			// copy contents, transforming it to explicit VR on the way, if necessary
			switch (zz->current.vr)
			{
			case SQ:
			case UN:
			case OB:
			case OW:
			case OF:
			case UT:
				explicit2(zw->fp, group, element, vr, len);
				break;
			default:
				explicit1(zw->fp, group, element, vr, len);
				break;
			}
			fwrite(addr + zz->current.pos, len, 1, zw->fp);	// write from mmap backing
		}
		else if (element == 0)
		{
			explicit1(zw->fp, group, element, "UL", 0);
		}

		// Read out valuable info
		switch (key)
		{
		case DCM_BitsStored:
			break;
		case DCM_BitsAllocated:
			params.bitspersample = zzgetuint16(zz, 0);
			break;
		case DCM_PhotometricInterpretation:
			// TODO, CS. need to be "MONOCHROME1", "MONOCHROME2" or "RGB" (or "PALETTE COLOR"?)
			break;
		case DCM_PixelRepresentation:
			// TODO, US; 0 - unsigned, 1 - signed
			break;
		case DCM_Rows:
			params.height = zzgetuint16(zz, 0);
			break;
		case DCM_Columns:
			params.width = zzgetuint16(zz, 0);
			break;
		case DCM_SamplesPerPixel:
			params.components = zzgetuint16(zz, 0);
			if (params.components == 3)
			{
				params.ilv = ILV_SAMPLE; // RGB
			}
			else
			{
				params.ilv = ILV_NONE; // grayscale
			}
			break;
		case DCM_PlanarConfiguration:
			if (zzgetuint16(zz, 0) != 0)
			{
				fprintf(stderr, "%s unsupported planar configuration\n", filename);
				return false;
			}
			break;
		case DCM_PixelData:
			src = addr + zz->current.pos; // malloc(len);
			dst = malloc(len);	// assume that we will not compress worse
			//params.colorTransform = 1;
			//params.allowedlossyerror = 3;
			params.bytesperline = 0;
			//printf("Encoding %dx%d image with %d bits per sample, %d samples per pixel\n",
			//       params.width, params.height, params.bitspersample, params.components);
			result = 0;
			err = JpegLsEncode(dst, len, &result, src, len, &params);
			switch (err)
			{
			case OK: break;
			case TooMuchCompressedData: fprintf(stderr, "%s - too much compressed data\n", filename); break;
			case InvalidJlsParameters: fprintf(stderr, "%s - invalid encoding parameters\n", filename); break;
			case ParameterValueNotSupported: fprintf(stderr, "%s - not supported encoding parameters\n", filename); break;
			case UncompressedBufferTooSmall: fprintf(stderr, "%s - could not yield compression gain\n", filename); break;
			case InvalidCompressedData:
			case ImageTypeNotSupported:
			case CompressedBufferTooSmall: fprintf(stderr, "%s - this should not happen\n", filename); break;
			case UnsupportedBitDepthForTransform: fprintf(stderr, "%s - unsupported bit depth\n", filename); break;
			case UnsupportedColorTransform: fprintf(stderr, "%s - unsupported color transform\n", filename); break;
			};
			zw->ladder[zw->ladderidx].txsyn = ZZ_EXPLICIT_JPEGLS;
			zzwPixelData_begin(zw, 1, 8, UNLIMITED);
			zzwPixelData_frame(zw, 0, dst, result);
			zzwPixelData_end(zw);
			free(dst);
			munmap(addr, zz->fileSize);
			zz = zzclose(zz);
			zw = zzclose(zw);
			return true;	// no, do not bother with any padding after the pixel data
			break;
		default:
			break;
		}
	}

	// FIXME : we should not really get here... handle it better, ideally remove file and warn user
	munmap(addr, zz->fileSize);
	zz = zzclose(zz);
	zw = zzclose(zw);
	return true;
}

int main(int argc, char **argv)
{
	int i;

	for (i = zzutil(argc, argv, 2, "<filenames>", "Convert uncompressed to JPEG-ls compressed DICOM", NULL); i < argc; i++)
	{
		jpegtols(argv[i]);
	}

	return 0;
}

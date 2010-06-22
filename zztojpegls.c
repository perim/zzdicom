#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>

#include "charlsintf.h"	// private port of CharLS pseudo-C interface to real C interface

#include "zz_priv.h"
#include "zzsql.h"

extern bool verbose;	// TODO, fix ugly global

static bool jpegtols(char *filename)
{
	struct JlsParamaters params;
	enum JLS_ERROR err;
	struct zzfile *zz = zzopen(filename, "r");
	struct stat st;
	size_t result;
	uint16_t group, element;
	char *src, *dst;
	uint32_t len, pos, size;
	char newname[PATH_MAX], *cptr;
	FILE *fp;

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
	fp = fopen(newname, "w");
	if (!fp)
	{
		fprintf(stderr, "%s - could not create out file: %s\n", newname, strerror(errno));
		return false;
	}

	memset(&params, 0, sizeof(params));
	while (!feof(zz->fp) && !ferror(zz->fp))
	{
		zzread(zz, &group, &element, &len);

		pos = ftell(zz->fp);

		// Copy (or alter) contents

		// Read out valuable info
		switch (ZZ_KEY(group, element))
		{
		case DCM_BitsStored:
			break;
		case DCM_BitsAllocated:
			params.bitspersample = zzgetuint16(zz);
			break;
		case DCM_PhotometricInterpretation:
			// TODO, CS. need to be "MONOCHROME1", "MONOCHROME2" or "RGB" (or "PALETTE COLOR"?)
			break;
		case DCM_PixelRepresentation:
			// TODO, US; 0 - unsigned, 1 - signed
			break;
		case DCM_Rows:
			params.height = zzgetuint16(zz);
			break;
		case DCM_Columns:
			params.width = zzgetuint16(zz);
			break;
		case DCM_SamplesPerPixel:
			params.components = zzgetuint16(zz);
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
			if (zzgetuint16(zz) != 0)
			{
				fprintf(stderr, "%s unsupported planar configuration\n", filename);
				return false;
			}
			break;
		case DCM_PixelData:
			src = malloc(len);
			dst = malloc(len);	// assume that we will not compress worse
			result = fread(src, len, 1, zz->fp);
			if (result != 1)
			{
				fprintf(stderr, "%s - bad image length (len %d != result %d)\n", filename, len, (int)result);
			}
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
			case InvalidJlsParameters: fprintf(stderr, "%s - invalid encoding parameters\n", filename); break;
			case ParameterValueNotSupported: fprintf(stderr, "%s - not supported encoding parameters\n", filename); break;
			case UncompressedBufferTooSmall: fprintf(stderr, "%s - could not yield compression gain\n", filename); break;
			case InvalidCompressedData:
			case ImageTypeNotSupported:
			case CompressedBufferTooSmall: fprintf(stderr, "%s - this should not happen\n", filename); break;
			case UnsupportedBitDepthForTransform: fprintf(stderr, "%s - unsupported bit depth\n", filename); break;
			case UnsupportedColorTransform: fprintf(stderr, "%s - unsupported color transform\n", filename); break;
			};
			free(src);
			free(dst);
			break;
		default:
			break;
		}

		// Skip ahead
		if (!feof(zz->fp) && len != 0xFFFFFFFF && len > 0)
		{
			fseek(zz->fp, pos + len, SEEK_SET);
		}
	}

	fclose(fp);
	zz = zzclose(zz);
	return true;
}

int main(int argc, char **argv)
{
	int i;

	for (i = zzutil(argc, argv, 2, "<filenames>", "Convert uncompressed to JPEG-ls compressed DICOM"); i < argc; i++)
	{
		jpegtols(argv[i]);
	}

	return 0;
}

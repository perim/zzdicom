#include <assert.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <sys/mman.h>
#include <CharLS/interface.h>

#include "zztexture.h"

#define checkError() assert(glGetError() == 0)

// TODO factorize this up a bit... it grew to monster size!
struct zztexture *zzcopytotexture(struct zzfile *zz, struct zztexture *zzt)
{
	uint16_t group, element;
	long len;
	int bitspersample = 0, components = 0;
	unsigned char *bytes;
	size_t length;
	GLuint textures[2]; // 0 - volume, 1 - volumeinfo
	GLenum type = GL_LUMINANCE16;
	GLenum size = GL_UNSIGNED_SHORT;
	char value[MAX_LEN_IS];
	double tmpd[6];
	GLfloat volinfo[256];	// volume info

	if (!zz || !zzt)
	{
		return NULL;
	}
	memset(volinfo, 0, sizeof(volinfo));
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_3D);
	glGenTextures(2, textures);
	glBindTexture(GL_TEXTURE_2D, textures[1]);
	checkError();
	zzt->volume = textures[0];
	zzt->volumeinfo = textures[1];
	zziterinit(zz);
	while (zziternext(zz, &group, &element, &len))
	{
		zzKey key = ZZ_KEY(group, element);

		// Read out valuable info
		switch (key)
		{
		case DCM_ImagePositionPatient:		// DS, 3 values
			zzrDS(zz, 3, tmpd);
			volinfo[9] = tmpd[0];
			volinfo[10] = tmpd[1];
			volinfo[11] = tmpd[2];
			break;
		case DCM_ImageOrientationPatient:	// DS, 6 values
			// Require multi-frame type DICOM here
			if (zzt->pixelsize.z == 0)
			{
				fprintf(stderr, "Number of frames not found before positions -- old style DICOM file?\n");
				return NULL;
			}
			zzrDS(zz, 6, tmpd);
			volinfo[0] = tmpd[0];
			volinfo[1] = tmpd[1];
			volinfo[2] = tmpd[2];
			volinfo[3] = tmpd[3];
			volinfo[4] = tmpd[4];
			volinfo[5] = tmpd[5];
			// space reserved for the normal vector (not sure if needed)
			break;
		case DCM_FrameOfReferenceUID:
			zzgetstring(zz, zzt->frameOfReferenceUid, sizeof(zzt->frameOfReferenceUid) - 1);
			break;
		case DCM_RescaleIntercept:	// DS, the b in m*SV + b
			zzgetstring(zz, value, sizeof(value) - 1);
			zzt->rescale.intercept = atof(value);
			break;
		case DCM_RescaleSlope:		// DS, the m in m*SV + b
			zzgetstring(zz, value, sizeof(value) - 1);
			zzt->rescale.slope = atof(value);
			break;
		case DCM_RescaleType:		// LO, but only two chars used; US -- unspecified, OD -- optical density, HU -- hounsfield units
			break;	// TODO
		case DCM_NumberOfFrames:
			zzgetstring(zz, value, sizeof(value) - 1);
			zzt->pixelsize.z = atoi(value);
			if (zzt->pixelsize.z <= 3)
			{
				fprintf(stderr, "Only multiframe images supported at this time.\n");
				return NULL;
			}
			// Reserve memory on the GPU
			glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, zzt->pixelsize.z, 64, 0, GL_LUMINANCE, GL_FLOAT, NULL);
			checkError();
			break;
		case DCM_BitsStored:
			break;
		case DCM_BitsAllocated:
			bitspersample = zzgetuint16(zz, 0);
			switch (bitspersample)
			{
			case 8: size = GL_UNSIGNED_BYTE; type = GL_LUMINANCE; break;
			case 16: size = GL_UNSIGNED_SHORT; type = GL_LUMINANCE16; break;
			default: fprintf(stderr, "Bad bit size!\n"); return NULL;
			}
			break;
		case DCM_PhotometricInterpretation:
			// TODO, CS. need to be "MONOCHROME1", "MONOCHROME2" or "RGB" (or "PALETTE COLOR"?)
			break;
		case DCM_PixelRepresentation:
			// TODO, US; 0 - unsigned, 1 - signed
			break;
		case DCM_Rows:
			zzt->pixelsize.x = zzgetuint16(zz, 0);
			break;
		case DCM_Columns:
			zzt->pixelsize.y = zzgetuint16(zz, 0);
			break;
		case DCM_SamplesPerPixel:
			components = zzgetuint16(zz, 0);
			if (components != 1)
			{
				fprintf(stderr, "Only grayscale images supported at this time.\n");
				return NULL;
			}
			break;
		case DCM_PlanarConfiguration:
			if (zzgetuint16(zz, 0) != 0)
			{
				fprintf(stderr, "Unsupported planar configuration\n");
				return NULL;
			}
			break;
		case DCM_SliceThickness:
			zzrDS(zz, 1, tmpd);
			volinfo[14] = tmpd[0];
			break;
		case DCM_PixelSpacing:
			zzrDS(zz, 2, tmpd);
			volinfo[12] = tmpd[0];
			volinfo[13] = tmpd[1];
			break;
		case DCM_Item:
		case DCM_PixelData:
			// Upload info from previous frame
			if (zz->current.frame > 0 && zz->current.pxstate == ZZ_PIXELITEM)
			{
				glTexSubImage2D(GL_TEXTURE_2D, 0, 0, zz->current.frame - 1, 64, 1, GL_LUMINANCE, GL_FLOAT, volinfo);
				checkError();
			}
			if (key == DCM_Item)
			{
				break;	// still got more frames to read
			}
			if (bitspersample == 0)	// by the time we get here, we need to have found all relevant pixel info
			{
				fprintf(stderr, "No valid image information found\n");
				return NULL;
			}
			glBindTexture(GL_TEXTURE_2D, 0);
			glBindTexture(GL_TEXTURE_3D, textures[0]);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
			glTexParameteri(GL_TEXTURE_3D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			checkError();
			// TODO - check actual pixel length against length of pixel data in file before reading
			length = zz->fileSize - zz->current.pos;
			bytes = zireadbuf(zz->zi, zz->current.pos, length);
			if (zz->ladder[zz->ladderidx].txsyn == ZZ_EXPLICIT_JPEGLS)
			{
				enum JLS_ERROR err;
				const long bufsize = zzt->pixelsize.x * zzt->pixelsize.y * components * (bitspersample / 8);
				unsigned char *buffer = malloc(bufsize), *src;
				long i = 0, start = zz->current.pos;
				glTexImage3D(GL_TEXTURE_3D, 0, type, zzt->pixelsize.x, zzt->pixelsize.y, zzt->pixelsize.z, 0, GL_LUMINANCE, size, NULL);
				// iterate into the encapsulated pixel data
				while (zziternext(zz, &group, &element, &len))
				{
					// TODO FIXME -- support fragmented items -- maybe remove padding (if charls cannot handle)
					if (ZZ_KEY(group, element) == DCM_Item && zz->current.pxstate == ZZ_PIXELITEM)
					{
						src = bytes + zz->current.pos - start;
						assert(src[0] == 0xff);
						assert(src[1] == 0xd8);
						err = JpegLsDecode(buffer, bufsize, src, len, NULL);
						assert(err == 0);
						glTexSubImage3D(GL_TEXTURE_3D, 0, 0, 0, i++, zzt->pixelsize.x, zzt->pixelsize.y, 1, GL_LUMINANCE, size, buffer);
						checkError();
					}
				}
				free(buffer);
			}
			else	// assume raw pixels
			{
				glTexImage3D(GL_TEXTURE_3D, 0, type, zzt->pixelsize.x, zzt->pixelsize.y, zzt->pixelsize.z, 0, GL_LUMINANCE, size, bytes);
			}
			checkError();
			zifreebuf(zz->zi, bytes, length);
			return zzt;
		}
	}

	return NULL;
}

struct zztexture *zztexturefree(struct zztexture *zzt)
{
	GLuint textures[2];

	textures[0] = zzt->volume;
	textures[1] = zzt->volumeinfo;

	glDeleteTextures(2, textures);
	return NULL;
}

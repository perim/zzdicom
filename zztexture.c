#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <errno.h>
#include <limits.h>
#include <sys/mman.h>
//#include "charlsintf.h"	// private port of CharLS pseudo-C interface to real C interface

#include "zztexture.h"

struct zztexture *zzcopytotexture(struct zzfile *zz, struct zztexture *zzt)
{
	uint16_t group, element;
	long len;
	int bitspersample = 0, components = 0;
	void *addr;
	off_t offset;
	size_t length;
	GLuint textures[2]; // 0 - volume, 1 - volumeinfo
	GLenum type, size;
	char value[MAX_LEN_IS];

	if (!zz || !zzt)
	{
		return NULL;
	}
	glEnable(GL_TEXTURE_2D);
	glEnable(GL_TEXTURE_3D);
	glGenTextures(2, textures);
	glBindTexture(GL_TEXTURE_2D, textures[1]);
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
		case DCM_ImageOrientationPatient:	// DS, 6 values
			// Require multi-frame type DICOM here
			if (zzt->pixelsize.z == 0)
			{
				fprintf(stderr, "Number of frames not found before positions -- old style DICOM file?\n");
				return NULL;
			}
			break; // TODO
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
			glTexImage2D(GL_TEXTURE_2D, 0, GL_LUMINANCE, zzt->pixelsize.z, 64, 0, GL_LUMINANCE, GL_FLOAT, NULL);
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
		case DCM_PixelData:
			if (bitspersample == 0)	// by the time we get here, we need to have found all relevant pixel info
			{
				fprintf(stderr, "No valid image information found\n");
				return NULL;
			}
			offset = zz->current.pos & ~(sysconf(_SC_PAGE_SIZE) - 1);	// start at page aligned offset
			length = zz->fileSize - zz->current.pos;			// FIXME - use actual pixel length
			// TODO - check actual pixel length against length of pixel data in file before reading
			addr = mmap(NULL, length, PROT_READ, MAP_SHARED, fileno(zz->fp), offset);
			if (addr == MAP_FAILED)
			{
				fprintf(stderr, "Could not memory map file: %s\n", strerror(errno));
				return NULL;
			}
			madvise(addr, length, MADV_SEQUENTIAL | MADV_WILLNEED);
			glBindTexture(GL_TEXTURE_3D, textures[0]);
			glTexImage3D(GL_TEXTURE_3D, 0, type, zzt->pixelsize.x, zzt->pixelsize.y, zzt->pixelsize.z, 0, GL_LUMINANCE, size, addr);
			madvise(addr, length, MADV_DONTNEED);
			munmap(addr, length);
		}
	}

	return zzt;
}

bool zztexturefree(struct zztexture *zzt)
{
	GLuint textures[2];

	textures[0] = zzt->volume;
	textures[1] = zzt->volumeinfo;

	glDeleteTextures(2, textures);
	return true;
}

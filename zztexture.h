#ifndef ZZ_TEXTURE_H
#define ZZ_TEXTURE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __APPLE__
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif
#include "zz_priv.h"

struct zztexture
{
	/// Slices are stored as a variety of texture types, all unsigned fixed point.
	/// The pixels are not transformed through the modality LUT.
	GLuint volume;

	/// Positions are stored as a 2D texture of grayscale floating point values,
	/// with each slice having a width of 256, height equal to number of frames.
	/// The data in each row contains: 9 values of orientation, 3 position, 3 
	/// voxel size, 3 reserved, and the remaining are LUT values. Each LUT is 
	/// stored as a sequence of floating point interval values. A simple linear
	/// LUT for example can be stored as two values. The LUT index values must
	/// be reverse transformed through the modality LUT before uploading. The
	/// LUT stream is ended on a zero value.
	GLuint volumeinfo;

	/// Frame of reference UID for coordinate space
	char frameOfReferenceUid[MAX_LEN_UI];

	/// Size of coordinate space
	struct
	{
		GLfloat x, y, z;
	} coordsize;

	/// Size of pixel space
	struct
	{
		int x, y, z;
	} pixelsize;

	/// Size of value space
	GLfloat maxValue;

	/// Modality LUT transform (an actual LUT is not supported though)
	struct
	{
		GLfloat intercept, slope;
	} rescale;

	struct
	{
		GLfloat level, window;
	} VOI;
};

/// Generate and upload 3D textures for visualization from a single multi-frame DICOM file. OpenGL
/// context must be initialized first.
struct zztexture *zzcopytotexture(struct zzfile *zz, struct zztexture *zzt);

/// Remove data from GPU memory
struct zztexture *zztexturefree(struct zztexture *zzt);

#ifdef __cplusplus
}
#endif

#endif

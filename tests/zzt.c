#include <assert.h>
#include <GL/glew.h>
#include <GL/freeglut.h>
#include "zztexture.h"

int main(int argc, char **argv)
{
	struct zzfile szz, *zz;
	struct zztexture stx, *tx;

	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s <volume>\n", argv[0]);
		return -1;
	}
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(0, 0);
	glutInitWindowSize(640, 480);
	glutCreateWindow("DICOM texture test");
	GLenum err = glewInit();
	assert(GLEW_OK == err);
	glutReportErrors();

	tx = zzcopytotexture(NULL, NULL);
	assert(tx == NULL);
	zz = zzopen(argv[1], "r", &szz);
	assert(zz != NULL);
	zzcopytotexture(zz, NULL);
	assert(tx == NULL);
	tx = zzcopytotexture(zz, &stx);
	assert(tx != NULL);
	assert(tx->pixelsize.x == 448);
	assert(tx->pixelsize.y == 448);
	assert(tx->pixelsize.z == 15);
	glutReportErrors();

	return 0;
}

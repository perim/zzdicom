#include <assert.h>
#ifdef __APPLE__
#include <GLUT/glut.h>
#else
#include <GL/freeglut.h>
#endif
#include "zztexture.h"

int main(int argc, char **argv)
{
	struct zzfile szz, *zz;
	struct zztexture stx, *tx;
	int win;

	if (argc != 2)
	{
		fprintf(stderr, "Usage: %s <volume>\n", argv[0]);
		return -1;
	}
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DEPTH | GLUT_DOUBLE | GLUT_RGBA);
	glutInitWindowPosition(0, 0);
	glutInitWindowSize(640, 480);
	win = glutCreateWindow("DICOM texture test");
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
	glutDestroyWindow(win);
	zz = zzclose(zz);

	return 0;
}

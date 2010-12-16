#include <assert.h>
#include "zztexture.h"

#define X11	// FIXME
#ifdef X11
#include <GL/glx.h>
#include <X11/Xlib.h>
#endif

int main(int argc, char **argv)
{
	struct zzfile szz, *zz;
	struct zztexture stx, *tx;
#ifdef X11
	GLXFBConfig *fbc;
	XVisualInfo *vi;
	int nelements;
	GLXContext cx;
	Display *dpy;
#endif

	(void)argc;
	(void)argv;

#ifdef X11
	dpy = XOpenDisplay(0);
	fbc = glXChooseFBConfig(dpy, DefaultScreen(dpy), 0, &nelements);
	vi = glXGetVisualFromFBConfig(dpy, fbc[0]);
	cx = glXCreateNewContext(dpy, fbc[0], GLX_RGBA_TYPE, 0, GL_FALSE);
#endif

	tx = zzcopytotexture(NULL, NULL);
	assert(tx == NULL);
	zz = zzopen("samples/tw2.dcm", "r", &szz);
	assert(zz != NULL);
	zzcopytotexture(zz, NULL);
	assert(tx == NULL);
	tx = zzcopytotexture(zz, &stx);

#ifdef X11
	// TODO cleanup
#endif

	return 0;
}

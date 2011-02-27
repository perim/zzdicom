* ----------------------------------------------------------------------------
* "THE BEER-WARE LICENSE"(Revision 42):
* < hiten@uk.FreeBSD.ORG > wrote this file.  As long as you retain this notice
* you can do whatever you want with this stuff. If we meet some day, and you
* think this stuff is worth it, you can buy me a beer in return. Hiten Pandya.
* ----------------------------------------------------------------------------

/*
 * Code contained herein, was inspired by Hanno Mueller's fmemopen
 * implementation for glibc.  But _NONE_ of the original code has
 * remained.
 */

/*
 * The famous fmemopen() string stream interface.  This interface is
 * used by various applications for doing nasty things, and it these
 * are very handy routines.  One example of usage of fmemopen() is
 * in the 'eet' code by Rasterman.
 *
 * Documentation for this interface is provided in the fmemopen(3)
 * manual page.  This code is still ALPHA quality under FreeBSD.
 *
 * Potential Problems:
 *
 * - This version of fmemopen() behaves differently than the original
 *   version.  I have never used the "original" interface.
 *
 * - The documentation doesn't say wether a string stream allows
 *   seeks. I checked the old fmemopen implementation in glibc's stdio
 *   directory, wasn't quite able to see what is going on in that
 *   source, but as far as I understand there was no seek there. For
 *   my application, I needed fseek() and ftell(), so it's here.
 *
 * - "append" mode and fseek(p, SEEK_END) have two different ideas
 *   about the "end" of the stream.
 *
 *   As described in the documentation, when opening the file in
 *   "append" mode, the position pointer will be set to the first null
 *   character of the string buffer (yet the buffer may already
 *   contain more data). For fseek(), the last byte of the buffer is
 *   used as the end of the stream.
 *
 * - It is unclear to me what the documentation tries to say when it
 *   explains what happens when you use fmemopen with a NULL buffer.
 *
 *   Quote: "fmemopen [then] allocates an array SIZE bytes long. This
 *   is really only useful if you are going to write things to the
 *   buffer and then read them back in again." -- glibc doc.
 *
 *   What does that mean if the original fmemopen() did not allow
 *   seeking? How do you read what you just wrote without seeking back
 *   to the beginning of the stream?
 *
 * - I think there should be a second version of fmemopen() that does
 *   not add null characters for each write. (At least in my
 *   application, I am not actually using strings but binary data and
 *   so I don't need the stream to add null characters on its own.)
 */

#include <sys/types.h>

#include <stdio.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* Private prototypes */
static int __fmemopen_closefn(void *);
static int __fmemopen_readfn(void *, char *, int);
static fpos_t __fmemopen_seekfn(void *, fpos_t, int);
static int __fmemopen_writefn(void *, const char *, int);
FILE *fmemopen(void *, size_t, const char *);

typedef struct {
	char *buffer;
	int mybuffer;
	size_t size;
	size_t pos;
	size_t maxpos;
} fmemopen_cookie;

static int
__fmemopen_readfn(void *cookie, char *buf, int len)
{
	fmemopen_cookie *c;
	c = (fmemopen_cookie*) cookie;

	/* #if 0  */
	if(c == NULL) {
		errno = EBADF;
		return -1;
	}
	/* #endif */

	if((c->pos + len) > c->size) {
		if(c->pos == c->size)
			return -1;
		len = c->size - c->pos;
	}

	memcpy(buf, &(c->buffer[c->pos]), len);

	c->pos += len;

	if(c->pos > c->maxpos)
		c->maxpos = c->pos;

	return len;
}


static int
__fmemopen_writefn(void *cookie, const char *buf, int len)
{
	fmemopen_cookie *c;
	int addnullc;

	c = (fmemopen_cookie*) cookie;
	if(c == NULL) {
		errno = EBADF;
		return -1;
	}

	addnullc = ((len == 0) || (buf[len - 1] != '\0')) ? 1 : 0;

	if((c->pos + len + addnullc) > c->size) {
		if((c->pos + addnullc) == c->size)
			return -1;
		len = c->size - c->pos - addnullc;
	}

	memcpy(&(c->buffer[c->pos]), buf, len);

	c->pos += len;
	if(c->pos > c->maxpos) {
		c->maxpos = c->pos;
		if(addnullc) c->buffer[c->maxpos] = '\0';
	}

	return len;
}


static fpos_t
__fmemopen_seekfn(void *cookie, fpos_t pos, int whence)
{
	fpos_t np = 0;
	fmemopen_cookie *c;

	c = (fmemopen_cookie*) cookie;

	switch(whence) {

	case SEEK_SET:
		np = pos;
		break;

	case SEEK_CUR:
		np = c->pos + pos;
		break;

	case SEEK_END:
		np = c->size - pos;
		break;

	}

	if((np < 0) || (np > c->size))
		return -1;

	c->pos = np;

	return np;
}


static
int __fmemopen_closefn(void *cookie)
{
	fmemopen_cookie *c;
	c = (fmemopen_cookie*) cookie;

	if(c->mybuffer)
		free(c->buffer);
	free(c);

	return 0;
}

FILE *fmemopen(void *buffer, size_t s, const char *mode)
{
	FILE *f = NULL;
	fmemopen_cookie *c;
	c = (fmemopen_cookie *) malloc(sizeof(fmemopen_cookie));

	if(c == NULL)
		return NULL;

	c->mybuffer = (buffer == NULL);

	if(c->mybuffer) {
		c->buffer = (char *) malloc(s);
		if(c->buffer == NULL) {
			free(c);
			return NULL;
		}
		c->buffer[0] = '\0';
	} else {
		c->buffer = buffer;
	}
	c->size = s;
	if(mode[0] == 'w')
		c->buffer[0] = '\0';
	c->maxpos = strlen(c->buffer);

	if(mode[0] == 'a')
		c->pos = c->maxpos;
	else
		c->pos = 0;

	/*
	 * We pass the various fmemopen_xxx routines to the funopen
	 * interface, and pray to god that it will do something about
	 * it.  It does not have the best style, but it is descriptive.
	 *
	 * The SEEK interface might be set to an invalidating routine
	 * because, the glibc documentation does not mention if seeks
	 * in a string stream are allowed or not.
	 */
	f = funopen(c,
	            __fmemopen_readfn, /* string stream read */
	            __fmemopen_writefn, /* string stream write */
	            __fmemopen_seekfn, /* string stream seek */
	            __fmemopen_closefn /* string stream loose :-) */
	           );

	if(f == NULL)
		free(c); /* Dont know if I am free'ing this second time */

	return f;
}

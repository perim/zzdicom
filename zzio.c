#include <stdlib.h>
#include <stdio.h>

#if defined(__linux__) || defined(__linux)
#define __USE_GNU
#define ZZ_LINUX
#include <sys/sendfile.h>
#endif

#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>

/* Safe MIN and MAX macros that only evaluate their expressions once. */
#undef MAX
#define MAX(a, b) \
	({ typeof (a) _a = (a); \
	   typeof (b) _b = (b); \
	 _a > _b ? _a : _b; })

#undef MIN
#define MIN(a, b) \
	({ typeof (a) _a = (a); \
	   typeof (b) _b = (b); \
	 _a < _b ? _a : _b; })

#include "zzio.h"

#define ZZIO_SOCKET	  1
#define ZZIO_READABLE     2
#define ZZIO_WRITABLE     4

struct zzio
{
	int fd;
	int flags;
	long readpos;		// where in the file we start our read buffer
	long writepos;		// where in the file we start our write buffer
	long readbufpos;	// where in our read buffer we are
	long writebufpos;	// where in our write buffer we are
	long readbuflen;	// amount of data in read buffer
	long writebuflen;	// amount of data in write buffer
	char *writebuf;		// write buffer
	char *readbuf;		// read buffer
	long bytesread;		// file size / bytes read on interface	TODO u64bit forced?
	long byteswritten;
	int error;
	long buffersize;

	// for header making
	int packetcount;
	long packetread;	// amount of data read since last packet header
	long packetwritten; 	// amount of data written since last packet header
	long headersize;
	headerwritefunc *writer;
	headerreadfunc *reader;
	void *userdata;
	char *header;
};

// TODO add zlib support here
// TODO while loop here
static inline long zi_read_raw(struct zzio *zi, void *buf, long len, bool wait)	// zi->readpos must be updated before calling
{
	long result;
	if (zi->flags & ZZIO_SOCKET)
	{
		// TODO MSG_DONTWAIT does not work on *BSD! need to set socket to non-blocking...
		result = recv(zi->fd, buf, len, wait ? MSG_WAITALL : MSG_DONTWAIT);
	}
	else
	{
		result = pread(zi->fd, buf, len, zi->readpos);
	}
	if (result == -1) printf("%s\n", strerror(errno));
	if (result > 0) zi->bytesread += result;
	return result;
}

// TODO add zlib support here
// TODO while loop here
static inline long zi_write_raw(struct zzio *zi, void *buf, long len)	// zi->writepos must be updated
{
	long result;
	if (zi->flags & ZZIO_SOCKET)
	{
		result = write(zi->fd, buf, len);
	}
	else
	{
		result = pwrite(zi->fd, buf, len, zi->writepos);
	}
	if (result == -1) printf("%s\n", strerror(errno));
	if (result > 0) zi->writepos += result;
	return result;
}

struct zzio *ziopenread(const char *path, int bufsize, int flags)
{
	struct zzio *zi = calloc(1, sizeof(*zi));
	zi->readbuf = calloc(1, bufsize);
	zi->flags = flags | ZZIO_READABLE;
#ifdef ZZ_LINUX
	zi->fd = open(path, O_RDONLY | O_NOATIME);
#else
	zi->fd = open(path, O_RDONLY);
#endif
	if (zi->fd == -1)
	{
		printf("%s\n", strerror(errno));
		return NULL;
	}
	zi->buffersize = bufsize;
	return zi;
}

struct zzio *ziopenwrite(const char *path, int bufsize, int flags)
{
	struct zzio *zi = calloc(1, sizeof(*zi));
	zi->writebuf = calloc(1, bufsize);
	zi->fd = creat(path, S_IRUSR | S_IWUSR | S_IRGRP);
	if (zi->fd == -1)
	{
		printf("%s\n", strerror(errno));
		return NULL;
	}
	zi->flags = flags | ZZIO_WRITABLE;
	zi->buffersize = bufsize;
	return zi;
}

struct zzio *ziopenmodify(const char *path, int bufsize, int flags)
{
	struct zzio *zi = calloc(1, sizeof(*zi));
	zi->readbuf = calloc(1, bufsize);
	zi->writebuf = calloc(1, bufsize);
	zi->fd = open(path, O_RDWR);
	if (zi->fd == -1)
	{
		printf("%s\n", strerror(errno));
		return NULL;
	}
	zi->flags = flags | ZZIO_WRITABLE | ZZIO_READABLE;
	zi->buffersize = bufsize;
	return zi;
}

struct zzio *ziopensocket(int socket, int bufsize, int flags)
{
	struct zzio *zi = calloc(1, sizeof(*zi));
	zi->fd = socket;
	zi->readbuf = calloc(1, bufsize);
	zi->writebuf = calloc(1, bufsize);
	zi->flags = flags | ZZIO_WRITABLE | ZZIO_READABLE | ZZIO_SOCKET;
	zi->buffersize = bufsize;
	return zi;
}

const char *zistrerror(void)
{
	return "error";
}

void zisplitter(struct zzio *zi, long headersize, headerwritefunc writefunc, headerreadfunc readfunc, void *userdata)
{
	zi->reader = readfunc;
	zi->writer = writefunc;
	zi->headersize = headersize;
	zi->userdata = userdata;
	zi->header = calloc(1, headersize);
}

//bool zisetflag(struct zzio *zi, int flag);	// turn compression on/off

long zireadpos(struct zzio *zi)
{
	return zi->readpos + zi->readbufpos;
}

long ziwritepos(struct zzio *zi)
{
	return zi->writepos + zi->writebufpos;
}

long zibyteswritten(struct zzio *zi)	// not including packet headers?
{
	return zi->byteswritten + zi->writebuflen;
}

long zibytesread(struct zzio *zi)	// not including packet headers?
{
	return zi->bytesread;
}

static inline void writeheader(struct zzio *zi, long length)
{
	long chunk;
	zi->writer(length, zi->header, zi->userdata);
	chunk = zi_write_raw(zi, zi->header, zi->headersize);
	if (chunk == -1) printf("%s\n", strerror(errno));
	assert(chunk == zi->headersize); // TODO FIXME loop
}

void ziflush(struct zzio *zi)
{
	long chunk;

	// commit writebuffer
	assert((zi->flags & ZZIO_WRITABLE) || zi->writebuflen == 0);
	if (zi->writer) writeheader(zi, zi->writebufpos);
	zi->writebufpos = 0;
	while (zi->writebuflen > 0)
	{
		chunk = zi_write_raw(zi, zi->writebuf + zi->writebufpos, zi->writebuflen);
		if (chunk > 0)
		{
			zi->writebuflen -= chunk;
			zi->writebufpos += chunk;
			zi->byteswritten += chunk;
		}
		assert(zi->writebufpos <= zi->buffersize && zi->writebufpos >= 0);
		assert(zi->writebuflen <= zi->buffersize);
	}
	assert(zi->writebuflen == 0);
	zi->writebufpos = 0;
	zi->writebuflen = 0;
}

// fd must be open already
// TODO - rename to ziaddfile? make it also work for files, not just sockets?
// TODO - test generating all heads at once, then iovec them + mmap'ed data, maybe faster?
long zisendfile(struct zzio *zi, int fd, long offset, long length)
{
	char *mem;
	long chunk = length, pos = offset;
	int flags;
	ssize_t result = 0;
	if (zi->writer) chunk = MIN(length, zi->buffersize);
#ifdef ZZ_LINUX
	// Fast implementation for socket sending on Linux
	if (zi->flags & ZZIO_SOCKET)
	{
		ziflush(zi);	// first empty write buffer, since we won't be using it
		do
		{
			if (zi->writer) writeheader(zi, chunk);
			while ((result = sendfile(fd, zi->fd, &pos, chunk)) == -1 && errno == EAGAIN) {}
			assert(result == chunk);
			chunk = MIN(length - (pos - offset), zi->buffersize);
		} while (chunk != 0); // chunk is zero when done
		assert(pos - offset == length);
		return pos - offset;
	}
#endif
	// Ok, now a general implementation for all remaining cases.
	flags = MAP_SHARED;
#ifdef ZZ_LINUX
	flags |= MAP_POPULATE | MAP_NONBLOCK;	// reserve memory for all pages already
#endif
	pos = offset & ~(sysconf(_SC_PAGE_SIZE) - 1); // offset for mmap() must be page aligned
	mem = mmap(NULL, length + offset - pos, PROT_READ, flags, fd, pos);
	if (mem != MAP_FAILED)
	{
		result = ziwrite(zi, mem + offset - pos, length);
	}
	munmap(mem, length + offset - pos);
	return result;
}

#if 0
void ziwrite2at(struct zzio *zi, uint16_t value);
void ziwrite4at(struct zzio *zi, uint32_t value);
#endif

void ziwillneed(struct zzio *zi, long offset, long length)
{
#ifdef ZZ_LINUX
	posix_fadvise(zi->fd, offset, length, POSIX_FADV_WILLNEED);
#endif
}

// TODO make fd into struct zzio
long zirecvfile(struct zzio *zi, int fd, long length)
{
#ifdef ZZ_LINUX
	posix_fallocate(zi->fd, zi->byteswritten, length);
#endif
	// readv on linux, recvfile on *BSD, append to fd
	return 0;
}

void ziputc(struct zzio *zi, int ch)
{
	if (zi->buffersize <= zi->writebufpos + 1)
	{
		ziflush(zi);
	}
	zi->writebuf[zi->writebufpos] = ch;
	zi->writebufpos++;
	zi->writebuflen++;
}

// Flush for the read buffer
static inline void zi_reposition_read(struct zzio *zi, long pos)
{
	zi->readpos = pos;
	zi->readbufpos = 0;
	zi->readbuflen = zi->buffersize;
	if (zi->reader)	// packetizer
	{
		zi_read_raw(zi, zi->header, zi->headersize, true);	// read header
		zi->readbuflen = zi->reader(zi->header, zi->userdata);	// it tells us the size of next packet
	}
	zi_read_raw(zi, zi->readbuf, zi->readbuflen, true);		// read next packet
}

int zigetc(struct zzio *zi)
{
	if (zi->readbuflen <= zi->readbufpos + 1)
	{
		zi_reposition_read(zi, zi->readpos + zi->readbufpos);
	}
	return zi->readbuf[zi->readbufpos++];
}

// TODO allow skipping forward, also in sockets and packetized input?
bool zisetreadpos(struct zzio *zi, long pos)
{
	if (pos > zi->readpos + zi->readbuflen || pos <  zi->readpos - (zi->readbuflen - zi->readbufpos))
	{
		// Seeking outside of buffer
		if ((zi->flags & ZZIO_SOCKET) || zi->reader) return false;	// TODO make error message
		zi_reposition_read(zi, pos);
	}
	zi->readpos = pos;
	return true;
}

bool zisetwritepos(struct zzio *zi, long pos)
{
	if (pos > zi->writepos + zi->writebuflen || pos <  zi->writepos - (zi->writebuflen - zi->writebufpos))
	{
		// Seeking outside of buffer
		if (zi->flags & ZZIO_SOCKET) return false;
		ziflush(zi);
		zi->writepos = pos;
	}
	zi->writepos = pos;
	return true;
}

long ziread(struct zzio *zi, void *buf, long count)
{
	long len;

	do
	{
		// Read as much as we can from buffer
		len = MIN(count, zi->readbuflen - zi->readbufpos);
		memcpy(buf, zi->readbuf + zi->readbufpos, len);
		zi->readbufpos += len;

		// Is buffer empty now and we need more?
		len = count - len;
		if (len > 0)	// yes, read in more
		{
			zi_reposition_read(zi, zi->readpos + zi->readbufpos);
			// TODO - optimize if no packetizer, by reading remainder raw? see ziwrite
		}
	} while (len > 0);
	return count;
}

long ziwrite(struct zzio *zi, void *buf, long count)
{
	long len;

	do
	{
		// Write as much as we can into buffer
		len = MIN(count, zi->buffersize - zi->writebufpos);
		memcpy(zi->writebuf + zi->writebufpos, buf, len);
		zi->writebufpos += len;
		zi->writebuflen += len;

		// Is buffer full now and we need more?
		len = count - len;
		if (len > 0)
		{
			ziflush(zi); // buffer blown, so flush it
#if 0
			if (!zi->writer)	// we can optimize and circumvent the buffer
			{
				long chunk = zi_write_raw(zi, buf + count - len, len);	// dump new content straight to file descriptor
				len -= chunk;
				zi->byteswritten += chunk;
			}
#endif
		}
	} while (len > 0);
	return count;
}

struct zzio *ziclose(struct zzio *zi)
{
	ziflush(zi);
	close(zi->fd);
	free(zi->readbuf);
	free(zi->writebuf);
	free(zi->header);
	free(zi);
	return NULL;
}

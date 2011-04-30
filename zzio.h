// High-performance buffered IO layer with transparent support for
// packet splitting and zlib compression. Note: Do NOT mix write
// calls to this interface with write calls through other inferfaces
// to the same file. Do not mix read ro write calls to this interface
// with read calls through other interfaces to the same socket.

#ifndef ZZIO_H
#define ZZIO_H

#if ( defined  __cplusplus ) || ( __STDC_VERSION__ < 199901L )
#define restrict
#endif

#include <stdlib.h>
#include <stdbool.h>
#include <stdint.h>

struct zzio;

// flags
#define ZZIO_ZLIB         8

/// Header write function. First parameter is number of bytes to be written in next packet.
/// Second parameter is the buffer to write into. Third parameter is user supplied.
typedef void headerwritefunc(long, char *, const void*);
/// Header read function. First parameter is buffer to read from. Second parameter is user
/// supplied. Returns number of bytes in next packet.
typedef long headerreadfunc(char *, void *);

struct zzio *ziopenread(const char *path, int bufsize, int flags);
struct zzio *ziopenwrite(const char *path, int bufsize, int flags);
struct zzio *ziopenmodify(const char *path, int bufsize, int flags);
struct zzio *ziopensocket(int sock, int bufsize, int flags);

/// Set up packet splitter that turns a stream of data into neat packets with custom headers. Max length of packet
/// must be equal to size of buffer as told earlier to ziopen*().
void zisplitter(struct zzio *zi, long headersize, headerwritefunc writefunc, headerreadfunc readfunc, void *userdata);

const char *zistrerror(void);
bool zisetflag(struct zzio *zi, int flag);	// turn compression on/off
long zireadpos(struct zzio *zi);
long ziwritepos(struct zzio *zi);
void ziflush(struct zzio *zi);
bool zisetreadpos(struct zzio *zi, long pos);
bool zisetwritepos(struct zzio *zi, long pos);
int zigetc(struct zzio *zi);
void ziputc(struct zzio *zi, int ch);
long ziread(struct zzio *zi, void *buf, long count);
long ziwrite(struct zzio *zi, void *buf, long count);
long zisendfile(struct zzio *zi, int fd, long offset, long length);	// test generating all heads and tails at once, then iovec them + mmap'ed data, maybe faster than sendfile()
long zirecvfile(struct zzio *zi, int fd, long length);	// readv on linux, recvfile on *BSD, append to fd
void ziwillneed(struct zzio *zi, long offset, long length);
void ziwrite2at(struct zzio *zi, uint16_t value);
void ziwrite4at(struct zzio *zi, uint32_t value);
struct zzio *ziclose(struct zzio *zi);	// returns NULL

// void zirepeat(struct zzio *zi, int ch, long num);	// repeat character ch num times (use memset in buffer, repeatedly if necessary)

#endif

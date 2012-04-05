#ifndef ZZ_NETWORK_H
#define ZZ_NETWORK_H

#include "zz_priv.h"

struct zznetwork
{
	struct zzfile *in;
	struct zzfile *out;
	struct zzfile *trace;
	bool server;
};

#define ZZNET_NO_FORK 0x0001

/// Program execution is stopped while waiting for network connections. If a connection is received, it is
/// accepted, then the program will fork. The child continues execution after this call, while the parent
/// continues to wait for new connections. If the ZZNET_NO_FORK flag is given, then fork will not be called,
/// instead this call will return on the first connection. This is useful for testing. Returns a read
/// handle.
struct zznetwork *zznetlisten(const char *interface, int port, int flags);

/// Connect to given host. Returns a read handle.
struct zznetwork *zznetconnect(const char *interface, const char *host, int port, int flags);

/// Clean up, close down, free memory.
struct zznetwork *zznetclose(struct zznetwork *zzn);

/// Add trace file.
void zznettrace(struct zznetwork *zzn, const char *filename);

#endif

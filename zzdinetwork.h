#ifndef ZZ_DINETWORK_H
#define ZZ_DINETWORK_H

#include "zz_priv.h"

/// Initialize a network interface
struct zzfile *zzdinetwork(const char *interface, const char *myaetitle, struct zzfile *zz);

/// Negotiate a Direct DICOM network connection
void zzdinegotiation(struct zzfile *zz);

#endif

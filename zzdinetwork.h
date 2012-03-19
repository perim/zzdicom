#ifndef ZZ_DINETWORK_H
#define ZZ_DINETWORK_H

#include "zz_priv.h"

/// Negotiate a Direct DICOM network connection
bool zzdinegotiation(struct zzfile *zz, bool server, struct zzfile *tracefile);

// debug functions
void zzdisending(struct zzfile *zz);
void zzdireceiving(struct zzfile *zz);

#endif

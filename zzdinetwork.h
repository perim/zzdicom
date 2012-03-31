#ifndef ZZ_DINETWORK_H
#define ZZ_DINETWORK_H

#include "zz_priv.h"

/// Negotiate a Direct DICOM network connection
bool zzdinegotiation(struct zzfile *zz, bool server, struct zzfile *tracefile);

// debug functions
void zzdisending(struct zzfile *zz, struct zzfile *tracefile);
void zzdireceiving(struct zzfile *zz, struct zzfile *tracefile);

#endif
